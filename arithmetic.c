/*
  arithmetic.c
 */

extern unsigned char N;
extern unsigned char Z;
extern unsigned char V;

unsigned char do_add_binary (unsigned char A, unsigned char arg, unsigned char * C)
{
  short sA;
  short sB;
  short sS;

  unsigned short uA;
  unsigned short uB;
  unsigned short uS;

  sA = (short)(signed char)A;
  sB = (short)(signed char)arg;

  sS = sA + sB;
  if (*C) sS++;

  V = (sS < -128 || sS > 127) ? 1 : 0;

  uA = (unsigned short)A;
  uB = (unsigned short)arg;

  uS = uA + uB;
  if (*C) uS++;

  *C = (uS > 255) ? 1 : 0;

  N = (uS & 0x80) ? 1 : 0;
  Z = (uS & 0xFF) ? 0 : 1;

  return (unsigned char)(uS & 0xFF);
}

unsigned char do_subtract_binary (unsigned char A, unsigned char arg, unsigned char * C)
{
  short sA;
  short sB;
  short sS;

  unsigned short uA;
  unsigned short uB;
  unsigned short uS;

  sA = (short)(signed char)A;
  sB = (short)(signed char)arg;

  sS = sA - sB;
  if (0 == *C) sS--;

  V = (sS < -128 || sS > 127) ? 1 : 0;

  uA = (unsigned short)A;
  uB = (unsigned short)arg;

  uS = uA - uB;
  if (0 == *C) uS--;

  *C = (uS > 255) ? 0 : 1;

  N = (uS & 0x80) ? 1 : 0;
  Z = (uS & 0xFF) ? 0 : 1;

  return (unsigned char)(uS & 0xFF);
}

unsigned char do_add_decimal (unsigned char A, unsigned char arg, unsigned char * C)
{
  unsigned char temp, result;

  /* add 'ones' digits */
  result = (*C) + (A & 0xF) + (arg & 0xF);

  /* carry if needed */
  temp = 0;
  if (result > 9)
    {
      temp = 1;
      result -= 10;
    }

  /* add 'tens' digits */
  temp += ((A >> 4) + (arg >> 4));

  *C = 0;
  if (temp > 9)
    {
      *C = 1;
      temp -= 10;
    }

  return result + (0x10 * temp);
}

unsigned char do_subtract_decimal (unsigned char A, unsigned char arg, unsigned char * C)
{
  unsigned char old_A = A;

  /* subtract 'ones' digits */
  A = A - (1-(*C)) - (arg & 0xF);

  /* if borrow was needed */
  if ((A & 0x0F) > (old_A & 0xF))
    {
      /* change 0x0F -> 0x09, etc. */
      A -= 6;
    }

  /* subtract 'tens' digits */
  A -= (arg & 0xF0);

  /* borrow if needed */
  if ((A & 0xF0) > (old_A & 0xF0))
    A -= 0x60;

  /* set flags */

  if (A > old_A)
    *C = 0;
  else
    *C = 1;

  return A;
}
