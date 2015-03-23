#include <stdio.h>

#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>

#define TILT_SERVO 0
#define PAN_SERVO 1

FILE *fp;

void closeServos( void )
{
 fclose(fp);
}


void servoblaster(int server, float  pulseWidth){
       fprintf(fp, "%i=%0.0f%\n",server,pulseWidth);
       fflush(fp);
}


void openServo()
{
    fp = fopen("/dev/servoblaster", "w");
    if (fp == NULL) {
        printf("Error opening /dev/servoblaster \n");
        exit(0);
    }
    //Set servos to neutral position
    servoblaster(TILT_SERVO,150.0);
    servoblaster(PAN_SERVO,150.0);
 
    }

void pantilt( float pan, float tilt )
{
	//Work out value as a percentage 
	servoblaster(TILT_SERVO,pan * 100);
	servoblaster(PAN_SERVO,tilt * 100);

}
