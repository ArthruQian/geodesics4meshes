/*
  rangecod.c     range encoding

  (c) Michael Schindler
  1997, 1998
  http://www.compressconsult.com/ or http://eiunix.tuwien.ac.at/~michael
  michael@compressconsult.com        michael@eiunix.tuwien.ac.at

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.  It may be that this
  program violates local patents in your country, however it is
  belived (NO WARRANTY!) to be patent-free here in Austria.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
  MA 02111-1307, USA.

  Range encoding is based on an article by G.N.N. Martin, submitted
  March 1979 and presented on the Video & Data Recording Conference,
  Southampton, July 24-27, 1979. If anyone can name the original
  copyright holder of that article or locate G.N.N. Martin please
  contact me; this might allow me to make that article available on
  the net for general public.

  Range coding is closely related to arithmetic coding, except that
  it does renormalisation in larger units than bits and is thus
  faster. An earlier version of this code was distributed as byte
  oriented arithmetic coding, but then I had no knowledge of Martin's
  paper from seventy-nine.

  The input and output is done by the INBYTE and OUTBYTE macros
  defined in the .c file; change them as needed; the first parameter
  passed to them is a pointer to the rangecoder structure; extend that
  structure as needed (and don't forget to initialize the values in
  start_encoding resp. start_decoding). This distribution writes to
  stdout and reads from stdin.

  There are no global or static var's, so if the IO is thread save the
  whole rangecoder is.

  For error recovery the last 3 bytes written contain the total number
  of bytes written since starting the encoder. This can be used to
  locate the beginning of a block if you have only the end.

  There is a supplementary file called renorm95.c available at the
  website (www.compressconsult.com/rangecoder/) that changes the range
  coder to an arithmetic coder for speed comparisons.
*/

/*
  This programm was modified (in particular for the contextual encoding functionalities)
  Laurent Demaret, 2006-2009
*/


/* define RENORM95 if you want the old renormalisation. Requires renorm95.c */
/* Note that the old version does not write out the bytes since init   */
/* #define RENORM95 */

/* define nowarn if you do not expect more than 2^32 outstanding bytes */
/* since I recommend restarting the coder in intervals of less than    */
/* 2^23 symbols for error tolerance this is not expected               */
#define NOWARN

#include <stdio.h>		//fprintf(), getc(), putc()
#include "port.h"
#include "rangecod.h"

/* SIZE OF RANGE ENCODING CODE VALUES. */

#define CODE_BITS 32
#define Top_value ((code_value)1 << (CODE_BITS-1))


#define SHIFT_BITS (CODE_BITS - 9)
#define EXTRA_BITS ((CODE_BITS-2) % 8 + 1)
#define Bottom_value (Top_value >> 8)

char coderversion[]="rangecode 1.1 (c) 1997, 1998 Michael Schindler";/* Start the encoder*/

// rc is the range coder to be used                            
// c is written as first byte in the datastream                
// one could do without c, but then you have an additional if  
// per outputbyte.                                             
void start_encoding( rangecoder *rc, char c )
{ 
  /*printf("(code_value)1 %d\n", (int) (code_value)1);
  printf("(CODE_BITS)1 %d\n", (int) (CODE_BITS-1));
  printf("Top_Value %d\n",  (unsigned int)Top_value); */
     
    rc->low = 0;                /* Full code range */
    rc->range = Top_value;
    rc->buffer = c;
    rc->help = 0;               /* No bytes to follow */
    rc->bytecount = 0;
    
    //printf("start_encoding: rc->range %d\n",  rc->range); 
}


#ifndef RENORM95
// I do the normalization before I need a defined state instead of 
// after messing it up. This simplifies starting and ending. 
static void enc_normalize( rangecoder *rc, WBitStream& OutBitStream)
{   
    while(rc->range <= Bottom_value)     /* do we need renormalisation?  */
    {   
        if (rc->low < 0xff<<SHIFT_BITS)  /* no carry possible --> output */
        {
            //outbyte(rc,rc->buffer,OutBitStream);
            OutBitStream.WriteByte(rc->buffer);
            for(; rc->help; rc->help--)
                //outbyte(rc,0xff,OutBitStream);
                OutBitStream.WriteByte(0xff);
            rc->buffer = (unsigned char)(rc->low >> SHIFT_BITS);
        } 
        else if (rc->low & Top_value) /* carry now, no future carry */
        {
            //outbyte(rc,rc->buffer+1,OutBitStream);
            OutBitStream.WriteByte(rc->buffer+1);
            for(; rc->help; rc->help--)
                // outbyte(rc,0,OutBitStream);
                OutBitStream.WriteByte(0);
            rc->buffer = (unsigned char)(rc->low >> SHIFT_BITS);
        } 
        else                           /* passes on a potential carry */
#ifdef WARNOVERFLOW
            if (rc->bytestofollow++ == 0xffffffffL)
            {   fprintf(stderr,"Too many bytes outstanding - File too large\n");
                exit(1);
            }
#else
            rc->help++;
#endif
        rc->range <<= 8;
        rc->low = (rc->low<<8) & (Top_value-1);
        rc->bytecount++;
    }
}

static void enc_normalize_debug( rangecoder *rc, WBitStream& OutBitStream)
{   
    while(rc->range <= Bottom_value)     /* do we need renormalisation?  */
    {   
        if (rc->low < 0xff<<SHIFT_BITS)  /* no carry possible --> output */
        {
            //outbyte(rc,rc->buffer,OutBitStream);
            OutBitStream.WriteByte(rc->buffer);
            for(; rc->help; rc->help--)
                //outbyte(rc,0xff,OutBitStream);
                OutBitStream.WriteByte(0xff);
            rc->buffer = (unsigned char)(rc->low >> SHIFT_BITS);
        } 
        else if (rc->low & Top_value) /* carry now, no future carry */
        {
            //outbyte(rc,rc->buffer+1,OutBitStream);
            OutBitStream.WriteByte(rc->buffer+1);
            for(; rc->help; rc->help--)
                // outbyte(rc,0,OutBitStream);
                OutBitStream.WriteByte(0);
            rc->buffer = (unsigned char)(rc->low >> SHIFT_BITS);
        } 
        else                           /* passes on a potential carry */
#ifdef WARNOVERFLOW
            if (rc->bytestofollow++ == 0xffffffffL)
            {   
                fprintf(stderr,"Too many bytes outstanding - File too large\n");
                exit(1);
            }
#else

            rc->help++;
#endif
        rc->range <<= 8;
        rc->low = (rc->low<<8) & (Top_value-1);
        rc->bytecount++;
    }    
}
#endif

using namespace std;



// Encode a symbol using frequencies                         
// rc is the range coder to be used                          
// sy_f is the interval length (frequency of the symbol)     
// lt_f is the lower end (frequency sum of < symbols)        
// tot_f is the total interval length (total frequency sum)  
// or (faster): tot_f = 1<<shift                             
void encode_freq( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f,
                 WBitStream& OutBitStream )
{	
    code_value r, tmp;
	enc_normalize( rc,OutBitStream);
	r = rc->range / tot_f;
	tmp = r * lt_f;
	if (lt_f+sy_f < tot_f)
		rc->range = r * sy_f;
	else
		rc->range -= tmp;
	rc->low += tmp;
}


void encode_shift(rangecoder *rc, freq sy_f, freq lt_f, freq shift,
                  WBitStream& OutBitStream)
{	
    code_value r, tmp;
    
	enc_normalize(rc,OutBitStream );

    r = rc->range >> shift;

	tmp = r * lt_f;


	if ((lt_f+sy_f) >> shift)
	{
      rc->range -= tmp;
    }
	else
	{
      rc->range = r * sy_f;
    }
	rc->low += tmp;    
}


void encode_shift_debug(rangecoder *rc, freq sy_f, freq lt_f, freq shift,
                  WBitStream& OutBitStream)
{	
    code_value r, tmp;
    
	enc_normalize_debug(rc,OutBitStream );

    r = rc->range >> shift;

    cout << "r : " << r << endl;

	tmp = r * lt_f;

    cout << "tmp : " << tmp << endl;

	if ((lt_f+sy_f) >> shift)
	{
      rc->range -= tmp;
    }
	else
	{
      rc->range = r * sy_f;
    }
	rc->low += tmp;
	
    
    cout << endl;
}

void dump(rangecoder* rc)
{
  cout << "rangecode.cpp : rc->dump()" << endl;   
  cout << "low: " << rc->low <<  " range: " << rc->range    << endl; 
  cout << "(int buffer )" << (int) rc->buffer <<  " bytecount : " << rc->bytecount << endl; 
  cout << endl;
}


#ifndef RENORM95
/* Finish encoding                                           */
/* rc is the range coder to be used                          */
/* actually not that many bytes need to be output, but who   */
/* cares. I output them because decode will read them :)     */
void done_encoding(rangecoder *rc, WBitStream& OutBitStream )
{   
    uint tmp;
    
    /*cout << "avant enc_normalize" << endl;
    dump(rc);*/
    
    enc_normalize(rc,OutBitStream);     /* now we have a normalized state */

    //cout << "Range0. OutBistream.Counter(): " << OutBitStream.GetCounter() << endl;;
    //dump(rc); 


    /*cout << "apres enc_normalize" << endl;
    dump(rc);*/

    rc->bytecount += 5;
    
    if ((rc->low & (Bottom_value-1)) < (rc->bytecount>>1))
       tmp = rc->low >> SHIFT_BITS;
    else
       tmp = (rc->low >> SHIFT_BITS) + 1;
       
    //cout << "tmp : " << tmp << endl;   
       
    if (tmp > 0xff) /* we have a carry */
    {
        //outbyte(rc, rc->buffer+1, OutBitStream);
        OutBitStream.WriteByte(rc->buffer+1);

        for(; rc->help; rc->help--)
            //outbyte(rc,0, OutBitStream);
            OutBitStream.WriteByte(0);
    } 
    else  /* no carry */
    {
        //outbyte(rc, rc->buffer, OutBitStream);
        OutBitStream.WriteByte(rc->buffer);
        for(; rc->help; rc->help--)
            //outbyte(rc,0xff, OutBitStream);
            OutBitStream.WriteByte(0xff);
    }

    //OutBitStream.WriteByte(tmp & 0xff);
    //remark: 0xff = 255
    OutBitStream.WriteByte(tmp & 255);

    
    /*cout << "Range1. OutBistream.Counter(): " << OutBitStream.GetCounter() << endl;;
    dump(rc); */

    OutBitStream.WriteByte((rc->bytecount>>16) & 255);
    
    OutBitStream.WriteByte((rc->bytecount>>8) & 0xff);
    
    OutBitStream.WriteByte(rc->bytecount & 0xff);
    
 }


// Start the decoder                                         
// rc is the range coder to be used                          
// returns the char from start_encoding or EOF   
int start_decoding( rangecoder *rc,RBitStream& InBitStream )
{
    //int c = inbyte(rc, InBitStream);
    int c = InBitStream.ReadChar();
    if (c==EOF)
        return EOF;
    rc->buffer = InBitStream.ReadChar();
    rc->low = rc->buffer >> (8-EXTRA_BITS);
    
    rc->range = 1 << EXTRA_BITS;
    
 
    return c;
}


static void dec_normalize( rangecoder *rc, RBitStream& InBitStream)
{   
  while (rc->range <= Bottom_value)
  {
    rc->low = (rc->low<<8) | ((rc->buffer<<EXTRA_BITS)&0xff);
    rc->buffer = InBitStream.ReadChar();
    rc->low |= rc->buffer >> (8-EXTRA_BITS);
    rc->range <<= 8;
        
  }  
}
#endif


/* Calculate cumulative frequency for next symbol. Does NO update!*/
/* rc is the range coder to be used                          */
/* tot_f is the total frequency                              */
/* or: totf is 1<<shift                                      */
/* returns the cumulative frequency                         */
freq decode_culfreq( rangecoder *rc, freq tot_f, RBitStream& InBitStream)
{   freq tmp;
    dec_normalize(rc, InBitStream);
    rc->help = rc->range/tot_f;
    tmp = rc->low/rc->help;
    return (tmp>=tot_f ? tot_f-1 : tmp);
}


freq decode_culshift( rangecoder *rc, freq shift, RBitStream& InBitStream)
{   
    freq tmp;
    dec_normalize(rc,InBitStream);
    rc->help = rc->range>>shift;
    tmp = rc->low/rc->help;
    
    return (tmp>>shift ? (1<<shift)-1 : tmp);
}


/* Update decoding state                                     */
/* rc is the range coder to be used                          */
/* sy_f is the interval length (frequency of the symbol)     */
/* lt_f is the lower end (frequency sum of < symbols)        */
/* tot_f is the total interval length (total frequency sum)  */
void decode_update( rangecoder *rc, freq sy_f, freq lt_f, freq tot_f)
{   
    code_value tmp;
    tmp = rc->help * lt_f;
    rc->low -= tmp;
    if (lt_f + sy_f < tot_f)
        rc->range = rc->help * sy_f;
    else
        rc->range -= tmp;
}


/* Decode a byte/short without modelling                     */
/* rc is the range coder to be used                          */
unsigned char decode_byte(rangecoder *rc, RBitStream& InBitStream)
{   unsigned char tmp = decode_culshift(rc,8,InBitStream);
    decode_update( rc,1,tmp,1<<8);
    return tmp;
}

unsigned short decode_short(rangecoder *rc,RBitStream& InBitStream)
{   unsigned short tmp = decode_culshift(rc,16,InBitStream);
    decode_update( rc,1,tmp,(freq)1<<16);
    return tmp;
}


// Finish decoding      
// rc is the range coder to be used                   
void done_decoding( rangecoder *rc, RBitStream& InBitStream)
{
  dec_normalize(rc,InBitStream);      /* normalize to use up all bytes */
}

