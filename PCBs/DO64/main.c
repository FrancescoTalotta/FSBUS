/********************************************************************
ATTINY2313
Programmed: SUT0  CKSEL1 
Revisions:
29.12.2006   row4 Fehler korrigiert
********************************************************************/
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
#define u08 unsigned char
#define u16 unsigned short
u08     IsSetupJumper(void);
void LED(u08 tm100, u08 count);
int ms100delay(u08 count);
volatile u08    cid;
volatile u08    x;
#define FLG_RESET    0x01
#define F_SHOWCID    0x02
volatile u08    flags = FLG_RESET;
#define sbi(p,b) p|=(1<<b)
#define cbi(p,b) p&=!(1<<b)
#define BV(x) (1<<x)
#define     EEPROM_CID        100
volatile u08     s_cid;
volatile u08     s_reg;
volatile u08     s_val;
volatile u08    s_cid;
volatile u08    s_seq = 0;
volatile u08     rxidx = 4;
volatile u08    bufs[8];
volatile u08    b;
volatile u08     a;
volatile u08     sr;

//*******************************************
int main(void)
{
    u08     i;
    CLKPR = BV(CLKPCE);
    CLKPR = 0;
    for (i=0; i<8; i++)
        bufs[i] = 0;
    for (;;)
    {
      for (i=0; i<8; i++) 
      {
        if (flags & FLG_RESET) 
        {
            flags &= ~FLG_RESET;
            cli();
            // IO Ports
            DDRD = 0x7C;        // alle Clock auf output
            PORTD = 0x00;
            DDRB = 0xff;        // alle data auf output
            // USART
            UBRRH = 0;
            UBRRL = 12; 
            UCSRA = 0;
            UCSRB = ( BV(RXCIE) | BV(RXEN) );
            //UCSRC = (BV(USBS) | BV(UCSZ1) | BV(UCSZ0));
            UCSRC = (1<<USBS)|(3<<UCSZ0);
            
            cid = eeprom_read_byte((uint8_t *)EEPROM_CID);
            if (cid > 30)
                cid = 30;
            flags |= F_SHOWCID;
            sei();        
        }
        if (flags & F_SHOWCID)
        {
            flags &= ~F_SHOWCID;
            b = cid / 10;
            LED(10, b);
            b = cid % (u08)10;
            LED(2, b);
        }
        cbi (PORTD, 2);            // disable 74138 
        PORTB = bufs[i];        // DATA ändern
        PORTD = (PIND & ~0x38) | (i << 3);  // Index ändern D3-D5
             
        sbi (PORTD, 2);            // enable  lo-hi stores
      }
    }    
    return 0;
}

//*******************************************
// USART Zeichen 
SIGNAL (SIG_USART0_RX)
{
    sr = UCSRA;
    b = UDR;
    if ( sr & (BV(FE)|BV(DOR)))
    {
        return;
    }
    if (b & 0x80)
        rxidx  = 0;
    switch (rxidx) 
    {
    case 0:
        a = (b & 0x7c) >> 2;
        if ((a != cid) && (a != 0)) 
        {
            rxidx = 4;
            return;
        }
        s_reg = (b & 0x02) << 6;
        s_val = b & 0x01;
        break;
        
    case 1:        // 2 BYTE KOMMANDOS
        s_reg |= b;
        switch (s_reg) 
        {
        case 128:        // RESET
            rxidx = 4;
            flags |= FLG_RESET;
            break;
        default:
            if (s_reg >= 88 && s_reg <= 119) 
            {    // DIGITAL OUT BIT
                a = (s_reg-88) / 8;        // byte 0-4
                b = (s_reg-88) % 8;     // bit 0-7
                if (s_val)
                    bufs[a] |= (1<<b);
                else
                    bufs[a] &= ~(1<<b);
                rxidx = 4;
            }
            if (s_reg >= 200 && s_reg <= 231) 
            {    // DIGITAL OUT BIT
                a = (s_reg-200) / 8;
                b = (s_reg-200) % 8;
                if (s_val)
                    bufs[a+4] |= (1<<b);
                else
                    bufs[a+4] &= ~(1<<b);
                rxidx = 4;
            }
            break;
        }
        break;
        
    case 2:        // 3 BYTE Kommandos
        s_val |= ((b & 0x7f) << 1);
        switch (s_reg) 
        {
        case 129:        // SetCID
            if (IsSetupJumper() == 0)
            {
                LED(1,3);
                break;
            }
        
            a = (s_val & 0xe0) >> 5;
            if (a == 0) 
            {
                s_seq = 0;
                s_cid = s_val & 0x1f;
                
            } else {
                if ((++s_seq) != a) 
                {
                    s_seq = 5;
                    break;
                }
                if (s_cid != (s_val & 0x1f)) 
                {
                    s_seq = 5;
                    break;
                }
                if (s_seq < 2)
                    break;
                eeprom_write_byte ((uint8_t *)EEPROM_CID, s_cid);
                flags |= FLG_RESET;
                rxidx = 4;
            }
            break;
            
        default:
            if (s_reg >= 120 && s_reg <= 123) 
            {    // DIGITAL OUT BYTE
                a = (s_reg-120);
                bufs[a] = s_val;
                rxidx = 4;
            }
            if (s_reg >= 232 && s_reg <= 235) 
            {    // DIGITAL OUT BYTE
                a = (s_reg-232);
                bufs[a+4] = s_val;
                rxidx = 4;
            }
            break;
        }
        break;
        
    //case 3:
    //    s_val |= ((b & 0x7f) << 8);
    //    break;
        
    default:
        rxidx = 4;
        break;
    }
    rxidx++;
}
//---------------------------------------------------------------------
u08 IsSetupJumper(void)
{
    u08 ret = 0;
    DDRB = 0x00;    // all in
    PORTB = 0x20;     // pullup PB5
    if ((PINB & 0x20) == 0)
        ret = 1;
    PORTB = 0x00;
    DDRB = 0xff;    // all out
    return ret;
}
//--------------------------------------------------------------------- 
void LED(u08 tm100, u08 count)
{
    while (count--)
    {
        PORTD = PIND | 0x40;    // LED an
        ms100delay(tm100);
        PORTD = PIND & ~0x40;    // LED aus
        ms100delay(tm100);
    }
}
//------------------------------------------
int ms100delay(u08 count)
{
    long x;
    int a;
    
    while (count--)
    {
        for (x=0; x<1500; x++)
            a = a * 23;
    }
    return a;
}
