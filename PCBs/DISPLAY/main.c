/********************************************************************
  FSBUS Display
  
  Dirk Anderseck  July 2004
  
  ATTINY2313, 4MHz  
  
17.4.05  R-Command 133 stores a local brightness base level on each display board
25.4.05  ATTINY version
*********************************************************************/
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#define BV(x)    (1<<x)
#define u08    unsigned char
#define u16    unsigned short
#define PORT_SEG    PORTB
#define PORT_DSP    PORTD
#define D1            6
#define D10            5
#define D100        4
#define D1000        3
#define D10000        2
#define D100000        1
#define CMD_RESET            128
#define CMD_SETCID            129     
#define CMD_BRIGHTNESS        130
#define CMD_POWER            131
#define CMD_DECIMALPOINT    132
#define CMD_BASEBRIGHT        133
volatile u08     rbuf[4];        // Code (0-15) einer Stelle rbuf[0] = rechtes Display
volatile u08     dbuf[6];
volatile u08        rxidx;
volatile u08     cid;            // CID dieses Bausteins
volatile u08     cmd;
volatile u08        val;
volatile u08     dp;                // position of dp (0 = off; 1 = rechtes Display)
volatile u08     dispix;            // Index der multiplexed displays
volatile u08     flags;
#define F_CC         0x01    // common anode
#define F_NEEDSAVE    0x02
#define F_SHOWCID    0x04
volatile u08     bright;
volatile u08     battery;
volatile u08     cidcnt;
volatile u08     rqcid;
u16     tickcount;
u08     dummy;
u08     ircv;
u08     basebright;
void     Reset(void);
int     Delay (int ms);
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
int main( void )
{
    int x;
    
    /*
     * ATTINY2313 das FUSE Bit CKDIV8 bestimmt den initialen Clock Scale
     * hier wird der Teilfaktor auf 0 gesetzt
     */
    CLKPR = BV(CLKPCE);
    CLKPR = 0;
    
    cid = eeprom_read_byte((uint8_t *)0);
    bright = 128;
    basebright = eeprom_read_byte((uint8_t *)2);
    battery = 100;
    rxidx = 99;
    
    if (eeprom_read_byte((uint8_t *)1) != (cid | 0x80)) 
    {
        cid = 31;
        basebright = 0;
    }
    tickcount = 0;
    // Timer 0 Prescaler
    // 0: timer stop 1: cpu clock 2: clk*8 3: clk*64 4: clk*256 5: clk*1024
    // Interrupt erfolgt nach Überlauf von TCNT0(8bit)
    TCCR0A = 0;
    TCCR0B = 2;    //  4ms
    TCNT0 = 0;            // Timer0 Counter     
    TIMSK = BV(TOIE0);        // Enable Interrupt
    //timer_enable_int(_BV(TOIE0));
    // UART
    UBRRH = 0;        // bps MSB
    UBRRL = 12;        // bps LSB
    UCSRA = 0;
    UCSRB = ( BV(RXCIE) | BV(RXEN) );    // enable receiver
    UCSRC = (BV(USBS) | BV(UCSZ1) | BV(UCSZ0)); // 8bit no parity, 2 Stopbit
    
    Reset();
    sei();           
    flags = F_SHOWCID;
    while (1) 
    {
        if (flags & F_SHOWCID) 
        {
            flags &= ~F_SHOWCID;
            for (x=0; x<sizeof(dbuf); x++)
                dbuf[x] = 0x0f;
            dp = 0;
            dbuf[1] = cid>=10 ? (cid / 10) : 15;
            dbuf[0] = cid % 10;
            Delay (1000);
            for (x=0; x<sizeof(dbuf); x++)
                dbuf[x] = 0x0f;
        }
        if (flags & F_NEEDSAVE) 
        {
            flags &= ~F_NEEDSAVE;
            eeprom_write_byte ((uint8_t *)0, cid);
            eeprom_write_byte ((uint8_t *)1, 0x80 | cid);
            eeprom_write_byte ((uint8_t *)2, basebright);
            Delay (1000);
            //flags = F_SHOWCID;
            Reset();
        }
    }
}
//---------------------------------------------------------------------
void Reset()
{
    dp = 0;
    dispix = 0;
    ircv = 7;
    rxidx = 255;
    
    DDRB = 0xff;
    DDRD = 0xff;    
}
//---------------------------------------------------------------------
int Delay (int ms)
{
    int cnt = 0;
    for (; ms; ms--)
        for (cnt = 0; cnt<10000;cnt++);
    return cnt;
}
//---------------------------------------------------------------------
SIGNAL (SIG_TIMER0_OVF)
{
    u08 x=0;
    u08 i=0;
    int ib;
#ifdef DSP_CA
    PORTB = 0xff;
#else
    PORTB = 0x00;
#endif
    ib = bright + 128 - basebright;
    if (ib < 0)
        ib = 0;
    if (ib > 255)
        ib = 255;
    tickcount++;
    if (tickcount & 1) {
        TCNT0 = ib;    
        return;
    }
    // Display    5 4 3 2 1 0
    // Index rbuf 5 4 3 2 1 0
    // PORTD      6 5 4 3 2 1
    if (++dispix > 5)
        dispix = 0;
#define SA            0x02
#define SB            0x08
#define SC            0x40
#define SD            0x20
#define SE            0x80
#define SF            0x01
#define SG            0x04
#define Sdp            0x10
#define S0            0x02
#define S1            0x04
#define S2            0x08
#define S3            0x10
#define S4            0x20
#define S5            0x40
    switch (dbuf[dispix]) 
    {
    case 0: x = SA|SB|SC|SD|SE|SF;         break;
    case 1: x = SB|SC;         break;
    case 2: x = SA|SB|SD|SE|SG;         break;
    case 3: x = SA|SB|SC|SD|SG;         break;
    case 4: x = SB|SC|SG|SF;             break;
    case 5: x = SA|SC|SD|SG|SF;         break;
    case 6: x = SF|SE|SG|SC|SD;         break;
    case 7: x = SA|SB|SC;                 break;
    case 8: x = SA|SB|SC|SD|SE|SF|SG;     break;
    case 9: x = SA|SB|SC|SG|SF;         break;
    case 10: x = SG;                     break;            // -
    case 11: x = SA|SC|SD|SG|SF;         break;        // S
    case 12: x = SD|SE|SF|SG;             break;      // t
    case 13: x = SB|SC|SD|SE|SG;         break;        // d
    case 14: x = SA|SD|SE|SF|SG;         break;        // E
    case 15: x = 0;                     break;
    }
    if (dispix == (dp-1))
        x |= Sdp;
    switch (dispix) {
    case 0: i = S0; break;        // least significant position
    case 1: i = S1; break;
    case 2: i = S2; break;
    case 3: i = S3; break;
    case 4: i = S4; break;
    case 5: i = S5; break;
    }
    if (battery > 80) 
    {
#ifdef DSP_CA
        PORTB = ~x;
        PORTD = i;
#else
        PORTB = x;
        PORTD = ~i;
#endif
    }
    TCNT0 = 255 - ib;    
}
//---------------------------------------------------------------------
SIGNAL (SIG_USART0_RX)
{
    u08 i;
    u08 x;
    u08 adr;
    
    i = UCSRA;
    x = UDR;
    if ((i & BV(FE)) || (i & BV(DOR)))    // Framing error or data overrun
        return;
    
    if (x & 0x80) 
    {
        // new block
        adr = (x & 0x7c) >> 2;
        cmd = (x & 0x02) ? 0x80 : 0;
        val = x & 0x01;
        
        if (adr && (adr != cid)) {
            rxidx = 255;
            return;
        }
        rxidx = 0;
        for (i=0; i<4; i++)
            rbuf[i] = 0xff;
        return;
    }
    if (rxidx >= 4)
        return;
    
    if (cmd & 0x80) 
    {
        // ordinary command
        if (++rxidx == 1) 
        {
            cmd |= x;
            return;
        }
        // 3. und letztes Byte
        val |= (x<<1);
            
        switch (cmd) 
        {
        case CMD_BASEBRIGHT:
            basebright = 255 - val;
            flags = F_NEEDSAVE;
            for (x=0; x<6; x++) 
            {
                dbuf[x] = val % 10;
                val /= 10;
            }
            break;
        case CMD_RESET:
            cli();
            if (val) 
                flags |= F_SHOWCID;
            Reset();
            sei();
            break;
            
        case CMD_SETCID:
            val &= 0x1f;
            if (cidcnt && (rqcid != val))
                cidcnt = 0;
                    
            dbuf[0] = 3 - cidcnt;                    
            dbuf[1] = 0x1f;                    
            dbuf[2] = val % 10;                    
            dbuf[3] = val / 10;                    
            dbuf[4] = 0x1f;                    
            dbuf[5] = 0x1f;                    
            rqcid = val;
            if (++cidcnt >= 3)
            {
                // SET NEW CID
                cid = rqcid;
                flags = F_NEEDSAVE; 
            }
            return;
            
        case CMD_BRIGHTNESS:    
            bright = val;
            break;
        case CMD_POWER:
            battery = val;
            break;
            
        case CMD_DECIMALPOINT:
            dp = val & 0x07;
            break;
        }
        cidcnt = 0;
        rxidx = 255;
    } else {
        // display 
        rbuf[rxidx++] = x;
        if (x & 0x40)  
        {
            // Display, copy to display buf
            dbuf[0] = ((rbuf[0] & 0x30) >> 2) | ((rbuf[1] & 0x30) >> 4);
            dbuf[1] = rbuf[0] & 0x0f;
            dbuf[2] = rbuf[1] & 0x0f;
            dbuf[3] = ((rbuf[2] & 0x30) >> 2) | ((rbuf[3] & 0x30) >> 4);
            dbuf[4] = rbuf[2] & 0x0f;
            dbuf[5] = rbuf[3] & 0x0f;
            return;
        }
        if (rxidx >= 4)
            rxidx = 255;
    }
}
