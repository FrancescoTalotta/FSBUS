/********************************************************************
ATMEGA 8535 16MHz
8k PROG
512 SRAM
512 EEPROM
PORT A   8 analogue inputs, digital driver out analog out
PORT B   8 digital driver output
PORT C   multiplexed keys input
PORT D  0 - Rx
        1 - Tx
        2 - Sense TX
        3 - C  Key row address 0-7
        4 - B  Key row address 0-7
        5 - A  Key row address 0-7
        6 - green LED 0-on
        7 - red LED 1-on
    
********************************************************************/
#include <avr/io.h>
#include <inttypes.h>
#include <avr/signal.h>
#include <avr/interrupt.h>
#define F_CPU 16000000 
#define UART_BPS 19200
#define u08 unsigned char
#define i08 signed char
#define u16 unsigned short
#define RED_ON         cbi(PORTD,6)
#define RED_OFF         sbi(PORTD,6)
#define GREEN_ON    sbi(PORTD,7)
#define GREEN_OFF    cbi(PORTD,7)
#define SENSE_IN     cbi(DDRD,2)
#define SENSE_OUT     sbi(DDRD,2)
#define SENSE_0      cbi(PORTD,2)
#define SENSE_1      sbi(PORTD,2)
#define RED            Red(3,10)
#define GREEN        Green(3,10)
void             Red (u08 cyles, u08 ms10);
void             Green (u08 cycles, u08 ms10);
void             WriteEEPROM (u16 uiAddress, u08 ucData);
u08             ReadEEPROM (u16 uiAddress);
void             Show (u08 value);
void            ScanKeys(void);
void            ScanAD (void);
void            Reset (void);
void             ScanTransmitter (void);
int             SetupEnabled(void);
void             ErrorLED(void);
volatile u08    cid;
volatile u08    us100;
volatile u08    ms10;
volatile u08    red10ms = 100;
volatile u08    redcnt = 0;
volatile u08    redphases = 0;
volatile u08    green10ms = 100;
volatile u08    greencnt = 0;
volatile u08    greenphases = 0;
volatile u08    keyidx = 0;            // von 0-64
volatile u08    keys[8];            // der letzte Zustand einer Row
volatile u08    keystate[64];        // Bit7 = senden
volatile u08    keytype[64];        // 
#define KT_BUTTON            1
#define KT_ROTARY_4            2
#define KT_ROTARY_2            3
#define KT_ROTARY_2PHASE    4
#define KT_ROTARY_2PHASE2    5
#define KT_BCD                6
#define KT_GRAYHILL            7
volatile u08    keyxmtidx = 0;        // ScanTransmitter zählt damit von 0-64             
volatile u08    xmitcmd;
volatile i08    xmitval;
volatile u08    adxmtidx = 0;        
volatile u08    adval[8];            // ermittelte Werte
volatile u08    adsend[8];            // zuletzt gesendet
volatile u08    adxmtflg = 0;        // bitweise, ob gesendet werden muss
volatile u08    adtolerance[8];        // Max. toleriertes Delta eines Kanals 
volatile u08    adidx = 0;
volatile u08    adenab = 0x00;            // jedes Bit zeigt einen Kanal an
#define         EE_CHKCLR        1
#define         EEPROM_CID        2
#define            EE_ADENAB        3
#define            EEPROM_AOUT        4
#define            EE_ADTOL        5
#define         EE_KEYTYPE        13
#define         ACT_RESET        0x01        // bei nächstem Loop Neustart
#define         ACT_SHOWCID        0x02        // Anzeige der CID mit LED 
#define            ACT_BUTTONSTATE    0x04
#define            ACT_WRITEEE        0x08
#define         ACT_RESET_AW    0x10        // reset after Write
#define         ACT_SHOWCID_AR    0x20        // show CID after reset
        
volatile u08    pflags;
volatile u08    xmt100uscount;

#define TX_IDLE            0
#define TX_WAITCID        1
#define TX_SCANSENSE    2
#define TX_WAITSENSE    3
#define TX_TRY            4
#define TX_TRY1            5
#define TX_SEND1        6
#define TX_SEND2        7
#define TX_SEND3        8
#define TX_FIN            9
volatile u08    txstate = TX_IDLE;
volatile u08     s_cid;
volatile u08     s_reg;
volatile u08     s_val;
volatile u08    s_cid;
volatile u08    s_seq = 0;
volatile u08     rxidx = 4;
volatile u08     adinconv = 0xff;
volatile u08    anaoutflg = 0;
volatile u08    aoutvalues[8];
volatile u08    aoutcounters[8];

//*******************************************
int main(void)
{
    int x;
    int txqueue = 0;
    pflags = ACT_RESET | ACT_SHOWCID;
//    if (bit_is_set(MCUCSR, PORF) || bit_is_set(MCUCSR, EXTRF))pflags |= ACT_SHOWCID;
    for (;;) 
    {
    
        if (pflags & ACT_WRITEEE) 
        {
            WriteEEPROM (EE_CHKCLR, 0x55);
            WriteEEPROM(EEPROM_CID, cid);
            WriteEEPROM (EE_ADENAB, adenab);
            WriteEEPROM (EEPROM_AOUT, anaoutflg);
            for (x=0; x<64; x++) 
                WriteEEPROM (EE_KEYTYPE + x, keytype[x]);
            for (x=0; x<8; x++) 
                WriteEEPROM (EE_ADTOL+x, adtolerance[x]);
            if (pflags & ACT_RESET_AW)
                pflags = ACT_RESET;
            pflags &= ~(ACT_WRITEEE|ACT_RESET_AW);
            keyxmtidx = 0;
            keyidx = 0;
        }
        if (pflags & ACT_RESET)   
        {
            Reset();
            if (pflags & ACT_SHOWCID_AR)
            {
                pflags |= ACT_SHOWCID;
                txqueue = 1;
            }
        }
        
        if (pflags & ACT_SHOWCID) {
            Show (cid);
            pflags &= ~(ACT_SHOWCID|ACT_SHOWCID_AR);
        }
        
        ScanKeys ();
        ScanAD ();
        if (txstate != TX_IDLE) {    
            ScanTransmitter ();
            continue;
        } 
        // suche Daten zum senden
        // zuerst Startsequenz beachten
        if (txqueue) {
            switch (txqueue) {
            case 1:
                xmitcmd = 255;            // this controller was resetted
                xmitval = 1;            // version of firmware
                break;
            case 2:
                xmitcmd = 124;        
                xmitval = adenab;    
                break;
            case 3:
                xmitcmd = 125;        
                xmitval = anaoutflg;
                break;
            
            default:
                if (txqueue >= 4 && txqueue < 12) {
                    xmitcmd = 128 + txqueue - 4;        
                    xmitval = adtolerance[txqueue - 4];
                    break;
                }
                txqueue = 0;
                continue;
            }
            txqueue++;
            txstate = TX_WAITSENSE;
            continue;
        }
        if (keystate[keyxmtidx] & 0x80) {
            Red (3, 2);                // zeigt Daten zum Senden an
            xmitcmd = keyxmtidx;
            switch (keytype[keyxmtidx]) {
            case KT_BCD:
                xmitval = keystate[keyxmtidx] & 0x0f;
                break;
            case KT_ROTARY_2:
            case KT_ROTARY_2PHASE:
            case KT_ROTARY_2PHASE2:
            case KT_ROTARY_4:
            case KT_GRAYHILL:
                xmitval = (i08)keystate[keyxmtidx+1];
                keystate[keyxmtidx+1] = 0;
                break;
            default:
            case KT_BUTTON:
                xmitval = (keystate[keyxmtidx] & 0x70) >> 4;
                break;
            }
            txstate = TX_WAITSENSE;
            keystate[keyxmtidx] = keystate[keyxmtidx] & 0x0f;
        }
        switch (keytype[keyxmtidx]) {
        case KT_ROTARY_2:
        case KT_ROTARY_2PHASE:
        case KT_ROTARY_2PHASE2:
        case KT_GRAYHILL:
            keyxmtidx += 2;
            break;
        default:
        case KT_BUTTON:
            keyxmtidx += 1;
            break;
        case KT_BCD:
        case KT_ROTARY_4:
            keyxmtidx += 4;
            break;
        }
        if (keyxmtidx >= 64)
            keyxmtidx = 0;
        if (txstate != TX_IDLE)
            continue;
        // jetzt AD converter prüfen
        if (++adxmtidx >= 8)
            adxmtidx = 0;
    
        if (adxmtflg & BV(adxmtidx)) {
            adxmtflg &= ~BV(adxmtidx);
            txstate = TX_WAITSENSE;
            xmitval = adsend[adxmtidx];
            xmitcmd = 72 + adxmtidx;
            Red (3, 2);                // zeigt Daten zum Senden an
        }
    }
}
//*******************************************
//*******************************************
//*******************************************
void ScanAD (void)
{
    unsigned int val;
    u08 a,b,i;
    
    if (bit_is_set(ADCSRA, ADSC))
        // a conversion is still in progress
        return;
    if (adinconv != 0xff) {
        // get last conversion result
        val = inp (ADCL);
        val = val | (inp (ADCH) << 8);
        a = val >> 2;
        b = adsend[adidx];
        if (a < b)
            i = b - a;
        else
            i = a - b;
        if (i > adtolerance[adidx]) {
            adval[adidx] = a; 
            adsend[adidx] = a;
            adxmtflg |= BV(adidx);
        }
        adinconv = 0xff;
        return;
    }
    if (++adidx > 7)
        adidx = 0;
    if (adenab & BV(adidx)) {
        adinconv = adidx;
        outp (0xc0 | adidx, ADMUX);    // int 2.56V Ref und mux channel
        sbi (ADCSRA, ADSC);                // starte Konvertierung
    }
}
/**************************************************************************
  Bei jedem Duchlauf wird 1 von 64 Tastern gescanned. Bit0-3 von Keystate
  zählen rauf oder runter und haben bei 0 bzw.4 den Sendezustand erreicht.
  Bit7 zeigt dieses an.
  Bit4-6 enthalten den zu sendenden Wert.  
**************************************************************************/
void ScanKeys()
{
    u08 row, bit, portbyte, ks, k;
    i08 v, sv, x;
    
    
    portbyte = inp (PINC);
    for (v=0; v<5; v++)
        if (portbyte != inp (PINC))
            return;
    row = keyidx / 8;
    bit = keyidx % 8;
    switch (keytype[keyidx]) {
    
    case KT_BCD:
        //Bit 0-3 = Wert  
        ks = keystate[keyidx] & 0x0f;            // Key State
        k = (~portbyte >> bit) & 0x0f;        // Key hardware
        if (ks != k) 
        {
            keystate[keyidx] = k;
            keystate[keyidx] |= 0x80;
        }
        keyidx += 4;
        break;
    case KT_ROTARY_4:
        ks = keystate[keyidx] & 0x0f;            // Key State
        k = ((~portbyte) & (0x0f << bit)) >> bit;        // Key hardware
        v=0;
        for (x=0; x<4; x++)
            if (k & BV(x))        // es darf nur 1 bit gesetzt sein
                v++;
        if (ks != k  && v == 1) {
            keystate[keyidx] = k;
            sv = (i08)keystate[keyidx+1];
            if (((k > ks) && !(k == 8 && ks == 1)) || (k == 1 && ks == 8))
                sv++;
            else
                sv--;
            keystate[keyidx] |= 0x80;
            keystate[keyidx+1] = (u08)sv;
        }
        keyidx += 4;
        break;
        
    case KT_ROTARY_2PHASE:
        ks = keystate[keyidx] & 0x03;                // Key State
        k = (portbyte & (0x03 << bit)) >> bit;        // Key hardware
        keystate[keyidx] = k;
        sv = (i08)keystate[keyidx+1];
        if ((ks & 0x01) && !(k & 0x01)) {
            if (k & 0x02) {
                sv++;
            } else {
                sv--;
            }
        }
        if (sv)
            keystate[keyidx] |= 0x80;
        keystate[keyidx+1] = (u08)sv;
        keyidx += 2;
        break;
    case KT_ROTARY_2PHASE2:
        ks = keystate[keyidx] & 0x03;                // Key State
        k = (portbyte & (0x03 << bit)) >> bit;        // Key hardware
        keystate[keyidx] = k;
        sv = (i08)keystate[keyidx+1];
        if ((ks & 0x01) && !(k & 0x01)) {
            if (k & 0x02) {
                sv++;
            } else {
                sv--;
            }
        }
        if (!(ks & 0x01) && (k & 0x01)) {
            if (k & 0x02) {
                sv--;
            } else {
                sv++;
            }
        }
        if (sv)
            keystate[keyidx] |= 0x80;
        keystate[keyidx+1] = (u08)sv;
        keyidx += 2;
        break;
    case KT_ROTARY_2:                // ALPS
        ks = keystate[keyidx] & 0x03;                // Key State
        k = (portbyte & (0x03 << bit)) >> bit;        // Key hardware
        keystate[keyidx] = k;
        sv = (i08)keystate[keyidx+1];
        if ((ks & 0x01) && !(k & 0x01) && (k & 0x02))
            sv++;
        if ((ks & 0x02) && !(k & 0x02) && (k & 0x01))
            sv--;
        if (sv)
            keystate[keyidx] |= 0x80;
        keystate[keyidx+1] = (u08)sv;
        keyidx += 2;
        break;
        
    case KT_GRAYHILL:
        ks = keystate[keyidx] & 0x03;                // Key State
        k = (portbyte & (0x03 << bit)) >> bit;        // Key hardware
        keystate[keyidx] = k;
        sv = (i08)keystate[keyidx+1];
        // ks enthält 2bit Zustand vom letzten Scan
        // k ist der jetzige Zustand
        // Beim Grayhill muss ein Wechsel beider Bits ausgewertet werden
        
        if ((k&1) && !(ks&1))    // pos edge bit0 
            sv += (k & 0x02) ? -1 : 1; 
        if (!(k&1) && (ks&1))    // neg edge bit0 
            sv += (k & 0x02) ? 1 : -1; 
                
        if ((k&2) && !(ks&2))    // pos edge bit1 
            sv += (k & 0x01) ? 1 : -1; 
        if (!(k&2) && (ks&2))    // neg edge bit1 
            sv += (k & 0x01) ? -1 : 1; 
                
        if (sv)
            keystate[keyidx] |= 0x80;
        keystate[keyidx+1] = (u08)sv;
        keyidx += 2;
        break;
    default:
    case KT_BUTTON:
        if (pflags & ACT_BUTTONSTATE) {
            // setze diese Taste invers, damit nächste scans den Wert senden
            if (portbyte & BV(bit))
                keystate[keyidx] = 0;
            else
                keystate[keyidx] = 4;
            keyidx++;
            break;
        }
        v = keystate[keyidx] & 0x07;
        sv = (keystate[keyidx] & 0x70) >> 4;
        if (portbyte & BV(bit)) {
            if ((v+1) == 4) {
                keystate[keyidx] |= 0x90;        // B7:markiert zum senden  B4:sende eine 1
                sv = 1;
            }
            if (v < 4)
                v++;
        } else {
            if ((v-1) == 0) {
                keystate[keyidx] |= 0x80;        // markiert zum senden
                sv = 0;
            }
            if (v > 0)
                v--;
        }
        keystate[keyidx] = v | (sv<<4) | (keystate[keyidx] & 0x80);
        keyidx++;
        break;
    }
    if (keyidx >= 64) {
        pflags &= ~ACT_BUTTONSTATE;
        keyidx = 0;
    }
    row = keyidx / 8;
    outp ((inp(PIND) & 0xc7)|(row << 3) , PORTD);
}

//**************************************************************************
void ScanTransmitter ()
{
    u08 x;
    switch (txstate) {
    case TX_WAITSENSE:
        if (bit_is_set(PIND, 2)) {        // sense is free
            txstate = TX_SCANSENSE;
            xmt100uscount = 4;            // check for free the next 400us
        }
        break;
    case TX_WAITCID:
        if (!xmt100uscount)
            txstate = TX_WAITSENSE;
        break;
        
    
    case TX_SCANSENSE:
        if (bit_is_clear(PIND, 2))    {    // held down by other controller
            txstate = TX_WAITSENSE;
            break;
        }
        if (xmt100uscount)                // test dauert 400us 
            break;
        SENSE_OUT;
        SENSE_0;                        // pull down SENSE
        txstate = TX_TRY;
        xmt100uscount = cid + 2;        // DEC by timer int
        break;    
    case TX_TRY:
        if (xmt100uscount)
            break;
        SENSE_IN;
        SENSE_1;        // Pullup aktivieren
        txstate = TX_TRY1;
        xmt100uscount = 2;
        break;
    case TX_TRY1:
        if (xmt100uscount)
            break;
        if (bit_is_set(PIND, 2)) {
            SENSE_OUT;
            SENSE_0;
            txstate = TX_SEND1;
        } else {
            txstate = TX_WAITCID;    // Das ganze nochmal
            xmt100uscount = cid+2;
        }
        break;
    case TX_SEND1:
        if ( bit_is_clear (UCSRA, UDRE))        // es wird noch gesendet
            break;
        x = 0x80 | (cid << 2) | ((xmitcmd >> 6) & 0x02) | (xmitval & 0x01);
        outp (x, UDR);    
        txstate = TX_SEND2;
        break;
            
    case TX_SEND2:
        if ( bit_is_clear (UCSRA, UDRE))
            break;
        x = xmitcmd & 0x7f;
        outp (x, UDR);    
        txstate = TX_SEND3;
        break;
            
    case TX_SEND3:
        if ( bit_is_clear (UCSRA, UDRE))
            break;
        outp ((xmitval>>1) & 0x7f, UDR);
        txstate = TX_FIN;
        break;
            
    case TX_FIN:
        if ( bit_is_clear (UCSRA, TXC))    // all shifted out
            break;
        txstate = TX_IDLE;
        SENSE_IN;
        SENSE_1;        // Pullup aktivieren
        sbi (UCSRA, TXC);        // to clear this flag
        break;
    }
}
//*******************************************
int SetupEnabled()
{
    // PORTD6 ist 0, wenn jumper gesetzt
    cbi (DDRD, 6);        // lese portd bit6
    if ( bit_is_clear (PIND, 6))
    {
        // ! PortD bleibt auf lesen
        return 1;
    }
    else 
    {
        sbi (DDRD, 6);        // rückschalten auf ausgabe led
        ErrorLED();
        return 0;
    }
}
//*******************************************
void Reset()
{
    u08 x;
    cli();
    // IO Ports
    cbi (SFIOR, PUD);                 // enable pullup in all ports
    outp (0xf8, DDRD);
    outp (0x40, PORTD);
    outp (0x00, DDRC);                // key input
    outp (0xff, PORTC);                // no pull up
    outp (0xff, DDRA);                // all portA = output
    outp (0xff, DDRB);                // all portB = output
    // Timer
    outp (2, TCCR0);                // TMR0 mit Clock/1024 
                                    // 1: 0 prescale
                                    // 2: 8 prescale
                                    // 3: 64 prescale
                                    // 4: 256 prescale
                                    // 5: 1024
    timer_enable_int (BV(TOIE0));    // Enable Timer0 Int
    // USART
    outp (0, UBRRH);
    outp (F_CPU / (UART_BPS * 16L) - 1, UBRRL);
    outp (BV(RXEN)|BV(TXEN)|BV(RXCIE), UCSRB);
    outp (0x80|(1<<USBS)|(3<<UCSZ0), UCSRC); // 2 Stop, 8Bit
    SENSE_IN;
    // ADC
    for (x=0; x<8; x++) {
        adsend[x] = 0xff;
        adtolerance[x] = ReadEEPROM (EE_ADTOL+x);
        if (adtolerance[x] == 0xff)
            adtolerance[x] = 0;
    }
    adidx = 0;
    adenab = 0;
    adinconv = 0xff;        // no channel in conversion
    if (ReadEEPROM (EE_CHKCLR) == 0x55) {
        adenab = ReadEEPROM (EE_ADENAB);
        anaoutflg = ReadEEPROM(EEPROM_AOUT);
    }
    outp (~adenab, DDRA);        // adenab bestimmt, ob in(0) or out(1)
    outp (0, PORTA);            // disable pullups
    outp ( BV(ADEN) | 7, ADCSRA);
    // Page 218:  16MHz/128 = 125khz
    outp (0xc0 | adidx, ADMUX);        
    // int 2.56V reference voltage; mux channel; input as single ended line
    for (x=0; x<64; x++) 
        keytype[x] = ReadEEPROM (EE_KEYTYPE + x);
    for (x=0; x<8; x++) 
        aoutvalues[x] = 128;

    cid = ReadEEPROM(EEPROM_CID);
    if (cid > 31)
        cid = 31;
    pflags &= ~ACT_RESET;
    pflags |= ACT_BUTTONSTATE;    // nächster Scan für Buttons stellt inverse Stati ein,
                                // damit folgende Scans ein Delta ergeben
    keyidx = 0;
    txstate = TX_IDLE;
    sei();        
}

//*******************************************
// USART hat ein Zeichen empfangen
SIGNAL (SIG_UART_RECV)
{
    u08 a;
    u08 b = inp(UDR);
    if (b & 0x80)
        rxidx  = 0;
    switch (rxidx) {
    case 0:
        a = (b & 0x7c) >> 2;
        if (a != cid && a != 0) {
            rxidx = 4;
            return;
        }
        Green (3, 10);
        s_reg = (b & 0x02) << 6;
        s_val = b & 0x01;
        break;
        
    case 1:        // 2 BYTE KOMMANDOS: Reset(128) DOoutBit(88-119)
        s_reg |= b;
        switch (s_reg) {
        case 128:        // RESET
            rxidx = 4;
            pflags |= ACT_RESET;
            pflags |= s_val ? ACT_SHOWCID_AR : 0;
            rxidx = 4;
            break;
        default:
            if (s_reg >= 88 && s_reg <= 119) {    // DIGITAL OUT BIT
                a = (s_reg-88) / 8;
                b = (s_reg-88) % 8;
                switch (a) {
                case 0:
                    if (!(adenab & BV(b))) {
                        if (s_val)
                            sbi(PORTA, b);
                        else
                            cbi(PORTA, b);
                    }
                    break;
                case 1:
                    if (s_val)
                        sbi(PORTB, b);
                    else
                        cbi(PORTB, b);
                    break;
                }
                rxidx = 4;
            }
            break;
        }
        break;
        
    case 2:        // 3 BYTE Kommandos
        s_val |= ((b & 0x7f) << 1);
        
        // Keytype (0-63)
        if (s_reg >= 0 && s_reg <= 63) {    
            if (!SetupEnabled())
                break;
            keytype[s_reg] = s_val;
            Green(3,100);
            pflags |= ACT_WRITEEE;
            break;
        }
        // ANALOG IN TOLERANZ (72-79) 
        if (s_reg >= 72 && s_reg <= 79) {    
            if (!SetupEnabled())
                break;
            a = s_reg - 72;
            adtolerance[a] = s_val;
            Green(3,100);
            pflags |= ACT_WRITEEE;
            break;
        }
        // ANALOG OUT (80-87)
        if (s_reg >= 80 && s_reg <= 87) {    
            a = s_reg - 80;
            aoutvalues[a] = s_val;
            break;
        }
        switch (s_reg) {
        case 129:        // SetCID count zählt 0,1,2
            if (!SetupEnabled())
                break;
            a = (s_val & 0xe0) >> 5;    // a ist counter
            if (a == 0) {
                s_seq = 0;
                s_cid = s_val & 0x1f;
            } else {
                if ((++s_seq) != a) {
                    Red(3,100);
                    rxidx = 4;
                    break;
                }
                if (s_seq < 2) {
                    rxidx = 4;
                    break;
                }
                cid = s_cid;
                Green(3,100);
                pflags = ACT_WRITEEE | ACT_RESET_AW | ACT_SHOWCID_AR ;
                rxidx = 4;
            }
            break;
            
        case 120:        // digi out port A
            PORTA = s_val & (~adenab);
            break;
            
        case 121:        // digi out port B
            PORTB = s_val & (~adenab);
            break;
            
        case 124:        // ANALOG INPUT MASK
            if (!SetupEnabled())
                break;
            Green(3,100);
            adenab = s_val;
            pflags = ACT_RESET_AW | ACT_WRITEEE;
            break;
        case 125:        // ANALOG OUTPUT MASK
            if (!SetupEnabled())
                break;
            Green(3,100);
            anaoutflg = s_val;
            pflags = ACT_RESET_AW | ACT_WRITEEE;
            break;
            
        }
        break;
        
    case 3:
        s_val |= ((b & 0x7f) << 8);
        break;
    }
    if (rxidx < 4)
        rxidx++;
}

//*******************************************
// TIMER0 Int
// GIE ist während der Funktion ausgeschaltet. Weiter Int finden danach statt
// Mit INTERRUPT werden andere Ints ermöglicht
SIGNAL (SIG_OVERFLOW0)
{
    u08 i;
    for (i=0; i<8; i++) {
        if (anaoutflg & BV(i)) {
            if (++aoutcounters[i] == 0)
                cbi(PORTA,i);
            if (aoutcounters[i] == aoutvalues[i])
                sbi(PORTA,i);
        }
    }    
    if (++us100 >= 100) {
        us100 = 0;
        ms10++;
        if (redphases) {
            if (--redcnt == 0) 
            {
                redcnt = red10ms;
                redphases--;
                cbi (DDRD, 6);        // lese portd bit6
                if ( bit_is_set (PIND, 6))
                {
                    sbi (DDRD, 6);        // rückschalten auf ausgabe led
                    if (!redphases || (redphases & 1))
                        RED_OFF;
                    else
                        RED_ON;
                }
            }
        }
        if (greenphases) {
            if (--greencnt == 0) {
                greencnt = green10ms;
                greenphases--;
                if (!greenphases || (greenphases & 1))
                    GREEN_OFF;
                else
                    GREEN_ON;
            }
        }
    }
    if (xmt100uscount)
        xmt100uscount--;
    TCNT0 = 64;    // alle 100 us erscheint Interupt
}

//*******************************************
void WriteEEPROM(u16 uiAddress, u08 ucData)
{
    cli();
    /* Wait for completion of previous write */
    
    while(EECR & (1<<EEWE));
    EEAR = uiAddress;
    EEDR = ucData;
    /* Write logical one to EEMWE */
    EECR |= (1<<EEMWE);
    /* Start eeprom write by setting EEWE */
    EECR |= (1<<EEWE);
    while(EECR & (1<<EEWE));
    sei();
}
//*******************************************
u08 ReadEEPROM(u16 uiAddress)
{
    cli();
    /* Wait for completion of previous write */
    while(EECR & (1<<EEWE));
    EEAR = uiAddress;
    /* Start eeprom read by writing EERE */
    EECR |= (1<<EERE);
    /* Return data from Data Register */
    sei();
    return EEDR;
}
//*******************************************
void Show (u08 value)
{
    Green (value/10*2+1, 100/*0ms*/);
    while (greenphases);
    Green ((value%10)*2+1, 15/*0ms*/);
    while (greenphases);
    Green (2, 100/*0ms*/);
    while (greenphases);
    pflags &= ~ACT_SHOWCID;
}
//*******************************************
void ErrorLED()
{
    Green(20, 5);
}
//*******************************************
void Red (u08 cycles, u08 ms10)
{
    red10ms = ms10;
    redcnt = 1;
    redphases = cycles;
}
//*******************************************
void Green (u08 cycles, u08 ms10)
{
    green10ms = ms10;
    greencnt = 1;
    greenphases = cycles;
}
