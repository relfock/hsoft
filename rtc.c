/*
 * Date & Time support (no alarms) for Dallas Semiconductor (now Maxim)
 * Extremly Accurate DS3231 Real Time Clock (RTC).
 *
 */

#include "rtc.h"
#define I2C_RTC_ADDR 0x68

//#define DEBUG_RTC
#include <i2c/i2c.h>
#include <string.h>
#include <stdio.h>

#ifdef DEBUG_RTC
#define DEBUGR(fmt,args...) printf(fmt ,##args)
#else
#define DEBUGR(fmt,args...)
#endif
/*---------------------------------------------------------------------*/

/*
 * RTC register addresses
 */
#define RTC_SEC_REG_ADDR	0x1
#define RTC_MIN_REG_ADDR	0x2
#define RTC_HR_REG_ADDR		0x3
#define RTC_DAY_REG_ADDR	0x4
#define RTC_DATE_REG_ADDR	0x5
#define RTC_MON_REG_ADDR	0x6
#define RTC_YR_REG_ADDR		0x7
#define RTC_CTL_REG_ADDR	0x0e
#define RTC_STAT_REG_ADDR	0x0f

/*
 * RTC control register bits
 */
#define RTC_CTL_BIT_A1IE	0x1	/* Alarm 1 interrupt enable     */
#define RTC_CTL_BIT_A2IE	0x2	/* Alarm 2 interrupt enable     */
#define RTC_CTL_BIT_INTCN	0x4	/* Interrupt control            */
#define RTC_CTL_BIT_RS1		0x8	/* Rate select 1                */
#define RTC_CTL_BIT_RS2		0x10	/* Rate select 2                */
#define RTC_CTL_BIT_DOSC	0x80	/* Disable Oscillator           */

/*
 * RTC status register bits
 */
#define RTC_STAT_BIT_A1F	0x1	/* Alarm 1 flag                 */
#define RTC_STAT_BIT_A2F	0x2	/* Alarm 2 flag                 */
#define RTC_STAT_BIT_OSF	0x80	/* Oscillator stop flag         */

#include <stdint.h>

static uint8_t rtc_read (uint8_t reg);
static void rtc_write (uint8_t reg, uint8_t val);
static uint8_t bin2bcd (unsigned int n);
static unsigned bcd2bin (uint8_t c);

/*
 * Get the current time from the RTC
 */
int rtc_get (struct rtc_time *tmp)
{
	int rel = 0;
	uint8_t sec, min, hour, mday, wday, mon_cent, year;

    /*
	 *control = rtc_read (RTC_CTL_REG_ADDR);
	 *status = rtc_read (RTC_STAT_REG_ADDR);
     */
	sec = rtc_read (RTC_SEC_REG_ADDR);
	min = rtc_read (RTC_MIN_REG_ADDR);
	hour = rtc_read (RTC_HR_REG_ADDR);
	wday = rtc_read (RTC_DAY_REG_ADDR);
	mday = rtc_read (RTC_DATE_REG_ADDR);
	mon_cent = rtc_read (RTC_MON_REG_ADDR);
	year = rtc_read (RTC_YR_REG_ADDR);

	DEBUGR ("Get RTC year: %02x mon/cent: %02x mday: %02x wday: %02x "
		"hr: %02x min: %02x sec: %02x control: %02x status: %02x\n",
		year, mon_cent, mday, wday, hour, min, sec, control, status);

	//if (status & RTC_STAT_BIT_OSF) {
	//	net_printf("### Warning: RTC oscillator has stopped\n");
	//	/* clear the OSF flag */
	//	rtc_write (RTC_STAT_REG_ADDR,
	//		   rtc_read (RTC_STAT_REG_ADDR) & ~RTC_STAT_BIT_OSF);
	//	rel = -1;
	//}

	tmp->tm_sec  = bcd2bin (sec & 0x7F);
	tmp->tm_min  = bcd2bin (min & 0x7F);
	tmp->tm_hour = bcd2bin (hour & 0x3F);
	tmp->tm_mday = bcd2bin (mday & 0x3F);
	tmp->tm_mon  = bcd2bin (mon_cent & 0x1F);
	tmp->tm_year = bcd2bin (year) + ((mon_cent & 0x80) ? 2000 : 1900);
	tmp->tm_wday = bcd2bin ((wday - 1) & 0x07);
	tmp->tm_yday = 0;
	tmp->tm_isdst= 0;

	DEBUGR("Get DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
		tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	return rel;
}


/*
 * Set the RTC
 */
void rtc_set (struct rtc_time *tmp)
{
	uint8_t century;

	DEBUGR("Set DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
		tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec);

	rtc_write (RTC_YR_REG_ADDR, bin2bcd (tmp->tm_year % 100));

	century = (tmp->tm_year >= 2000) ? 0x80 : 0;
	rtc_write (RTC_MON_REG_ADDR, bin2bcd (tmp->tm_mon) | century);

	rtc_write (RTC_DAY_REG_ADDR, bin2bcd (tmp->tm_wday + 1));
	rtc_write (RTC_DATE_REG_ADDR, bin2bcd (tmp->tm_mday));
	rtc_write (RTC_HR_REG_ADDR, bin2bcd (tmp->tm_hour));
	rtc_write (RTC_MIN_REG_ADDR, bin2bcd (tmp->tm_min));
	rtc_write (RTC_SEC_REG_ADDR, bin2bcd (tmp->tm_sec));
}


/*
 * Reset the RTC.  We also enable the oscillator output on the
 * SQW/INTB* pin and program it for 32,768 Hz output. Note that
 * according to the datasheet, turning on the square wave output
 * increases the current drain on the backup battery from about
 * 600 nA to 2uA.
 */
void rtc_reset (void)
{
	//rtc_write (RTC_CTL_REG_ADDR, RTC_CTL_BIT_RS1 | RTC_CTL_BIT_RS2);
}


/*
 * Helper functions
 */
static uint8_t rtc_read (uint8_t reg)
{
    uint8_t data;
    i2c_slave_read(I2C_RTC_ADDR, reg, &data, 1);
	return data;
}


static void rtc_write (uint8_t reg, uint8_t val)
{
    uint8_t d[] = { reg, val };
    i2c_slave_write(I2C_RTC_ADDR, d, 2);
}

static unsigned bcd2bin (uint8_t n)
{
	return ((((n >> 4) & 0x0F) * 10) + (n & 0x0F));
}

static uint8_t bin2bcd (unsigned int n)
{
	return (((n / 10) << 4) | (n % 10));
}
