/*
        A simple program that demonstrates how to use a touchscreen to
	control the pan and tilt of a camera. This is specifically on
	the Raspberry Pi, however this program can be used for most
	Linux based systems.
	For more details: ozzmaker.com

    Copyright (C) 2015  Mark Williams

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
    MA 02111-1307, USA
*/

#include <linux/input.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include "touch.h"
#include "touch.c"
#include "framebuffer.c"
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include "servo.c"


//used for button coordinates
#define X 0		//Top left corner X position
#define Y 1		//Top left corner Y position
#define W 2		//Button width
#define H 3		//Button height
#define L 4		//Slider length


void drawButton(int x, int y, int w, int h, char *text, int backgroundColor, int foregroundColor);
int mymillis();
void INThandler(int sig);
void videoCapture();

pid_t pid;

struct buttons
{
	int coords[5];
	int currentPosition[4];
	int timer;
	int delta;
	int oldScaledData[2]; 
};



int main()
{
	signal(SIGINT, INThandler);

	int  xres,yres;
	int screenXmax, screenXmin;
	int screenYmax, screenYmin;
	float scaleXvalue, scaleYvalue;
	int rawX, rawY, rawPressure, scaledX, scaledY;

	//Used to monitor time
	int timer = mymillis();


	int cameraOkayToUse = 1;

	//Our buttons.  X, Y, Width, Height,slide length, delta
	struct buttons button[4] = {
	//First button is used to control pan
	{	button[0].coords[X]=10,
		button[0].coords[Y]=239,
		button[0].coords[W]=80,
		button[0].coords[H]=80,
		button[0].coords[L]=200,
		button[0].delta=0},
	//Second button is used to control tilt
	{       button[1].coords[X]=399,
        	button[1].coords[Y]=30,
	        button[1].coords[W]=80,
	        button[1].coords[H]=80,
	        button[1].coords[L]=200,
		button[1].delta=0},
	//Third button is used to start recording
	{       button[2].coords[X]=305,
        	button[2].coords[Y]=239,
	        button[2].coords[W]=80,
	        button[2].coords[H]=35,
	        button[2].coords[L]=0,
		button[2].delta=0},
	//Fourth  button is used to start recording
	{       button[3].coords[X]=305,
        	button[3].coords[Y]=279,
	        button[3].coords[W]=80,
	        button[3].coords[H]=35,
	        button[3].coords[L]=0,
		button[3].delta=0}
	};

	//Open file handler used to write to /dev/servoblaster
	openServo();

	if (openTouchScreen() == 1)
		perror("error opening touch screen");

	//Get touchscreen details
	getTouchScreenDetails(&screenXmin,&screenXmax,&screenYmin,&screenYmax);

	//Open framebuffer
	framebufferInitialize(&xres,&yres);

	//Work out the values to scale touchscreen input
	scaleXvalue = ((float)screenXmax-screenXmin) / xres;
	printf ("X Scale Factor = %f\n", scaleXvalue);
	scaleYvalue = ((float)screenYmax-screenYmin) / yres;
	printf ("Y Scale Factor = %f\n", scaleYvalue);


	//Used to compare input readings
	int oldX = 0, oldY = 0;

        //So set the start position from the initial coordinates stored in the button structure
	button[0].currentPosition[X] = button[0].coords[X];
	button[0].currentPosition[Y] = button[0].coords[Y];
	button[1].currentPosition[X] = button[1].coords[X];
	button[1].currentPosition[Y] = button[1].coords[Y];




	//initialise Dispmanx used for framebuffer copying
	initFbcp();


	//Copy temp  buffer to buffer used by framebuffer.  ' * 2' because we are using a 16 bit display
	memcpy (fbp, backBufferP, var.xres * var.yres * 2);


	while(1){
		fbcp();

		//Draw slider outline. we can use the drawBotton() function for this, which takes the input drawButton(X,Y,W,H,text,outside colour,inside colour fill)
		drawButton(button[0].coords[X]-1, button[0].coords[Y]-1,			//one less from X and Y position
			button[0].coords[W]+button[0].coords[L]+2,				//button 0 is PAN, so we use the width of the button plus the length of the slide
			button[0].coords[H]+2,							//Add two to height so the outline appears outside our button
			"",RED,BLACK);								//No text, red outline and black fill

		drawButton(button[1].coords[X]-1, button[1].coords[Y]-1,			//one less from X and Y position
			button[1].coords[W]+2,							//Add two to width so the outline appears outside our button
			button[1].coords[H]+button[1].coords[L]+2,				//button 1 is tilt, so we use the height of the button plus the length of the slide
			"",RED,BLACK);								//No text, red outline and black fill


		//Start timer
		int time = mymillis();

		//Get and scale touch samples
                getTouchSample(&rawY, &rawX, &rawPressure);
                scaledX = rawX/scaleXvalue;
                scaledY = rawY/scaleYvalue;

#ifdef DEBUGSCALED
		printf("scaledX %i  scaledY %i\n", scaledX,scaledY);
#endif

		//###############PAN Button####################//
		//Check to see if current touch event is happening within our button area.
		//Does the current X input fall between the current X position and the width of the button?
		//Does the current Y input fall between the current Y position and the height of the button?
		if((scaledX  >  button[0].currentPosition[X]  && scaledX < ( button[0].currentPosition[X]  + button[0].coords[W]))
				&&
		(scaledY >  button[0].coords[Y] && scaledY < (button[0].coords[Y]+button[0].coords[H]))){
			//Update the scaled  Delta value
			button[0].delta -= button[0].oldScaledData[X] - scaledX;

			//Stor the new value for the next loop
			button[0].oldScaledData[X]=scaledX;

			//Update the current position based on the original coordinates with the added delta. (button[0].coords[W]/2) is used to offset the button to keep it centred.
			button[0].currentPosition[X] = button[0].coords[X] + button[0].delta  - (button[0].coords[W]/2);

			//Keep slider button within its sliding limit
			//If the current X position is greater than the original position plus the slider length, then set it to max slide length.
			if (button[0].currentPosition[X] > button[0].coords[X] + button[0].coords[L] )
				button[0].currentPosition[X] =  button[0].coords[X]  + button[0].coords[L] ;
			//If the current X position is smaller than the original, then set it to the back to the original position
			if (button[0].currentPosition[X] < button[0].coords[X])
				button[0].currentPosition[X] = button[0].coords[X];
	                //Draw button with a lighter colour to show that it has been pressed
			drawButton(button[0].currentPosition[X],button[0].currentPosition[Y],button[0].coords[W],button[0].coords[H],"PAN",GREY,LIGHT_BLUE);
		}
		else{
			//Redraw button
			drawButton(button[0].currentPosition[X],button[0].currentPosition[Y],button[0].coords[W],button[0].coords[H],"PAN",GREY,BLUE);
		}

		//###############TILT Button####################//
		if((scaledX  >  button[1].coords[X]  && scaledX < ( button[1].coords[X] + button[1].coords[W]))
				 &&
		 (scaledY >  button[1].currentPosition[Y] && scaledY < (button[1].currentPosition[Y]+button[1].coords[H]))){
			//Update the scaled  Delta value
			button[1].delta -= button[1].oldScaledData[Y] - scaledY;

			//Stor the new value for the next loop
			button[1].oldScaledData[Y]=scaledY;

			//Update the current position based on the original coordinates with the added delta.
	                button[1].currentPosition[Y] = button[1].coords[Y] + button[1].delta - button[1].coords[H];

	                //Keep slider buttons within its sliding limit
        	        //If the current Y position is greater than the original position plus the slider length, then set it to max slide length.
                	if (button[1].currentPosition[Y] > button[1].coords[Y] + button[1].coords[L] )
				button[1].currentPosition[Y] =  button[1].coords[Y]  + button[1].coords[L] ;
	                //If the current Y position is smaller than the original, then set it to the back to the original position
        	        if (button[1].currentPosition[Y] < button[1].coords[Y])
                	       button[1].currentPosition[Y] = button[1].coords[Y];

	                //Draw button with a lighter colour to show that it has been pressed
        	        drawButton(button[1].currentPosition[X],button[1].currentPosition[Y],button[1].coords[W],button[1].coords[H],"TILT",GREY,LIGHT_PURPLE);
		}
		else{
                	//Redraw button
			drawButton(button[1].currentPosition[X],button[1].currentPosition[Y],button[1].coords[W],button[1].coords[H],"TILT",GREY,PURPLE);
		}

		//###############Start recording Button####################
		if((scaledX  >  button[2].coords[X]  && scaledX < (button[2].coords[X] + button[2].coords[W]))
                	         &&
			(scaledY >  button[2].coords[Y] && scaledY < (button[2].coords[Y]+button[2].coords[H]))){
        	        //Draw button with a lighter colour to show that it has been pressed
			 drawButton(button[2].coords[X],button[2].coords[Y],button[2].coords[W],button[2].coords[H],"Start",GREY,LIGHT_GREEN);

			//Start to capture video , only if it is okay to do so.
			if ( cameraOkayToUse ){
				videoCapture();
				cameraOkayToUse = 0;
			}
		}
		else
			drawButton(button[2].coords[X],button[2].coords[Y],button[2].coords[W],button[2].coords[H],"Start",GREY,GREEN);


		//###############Stop recording Button####################
		if((scaledX  >  button[3].coords[X]  && scaledX < (button[3].coords[X] + button[3].coords[W]))
                	         &&
			(scaledY >  button[3].coords[Y] && scaledY < (button[3].coords[Y]+button[3].coords[H]))){
        	        //Draw button with a lighter colour to show that it has been pressed
			 drawButton(button[3].coords[X],button[3].coords[Y],button[3].coords[W],button[3].coords[H],"Stop",GREY,LIGHT_RED);

			//Here we kill the child process  created by videoCapture().   This stops recording
			if (pid != 0){
				kill(pid,SIGKILL);
				pid = 0;
				cameraOkayToUse = 1;
			}
		}
		else
			drawButton(button[3].coords[X],button[3].coords[Y],button[3].coords[W],button[3].coords[H],"Stop",GREY,RED);




		//Here we keep track of the time since a button was pressed. And change it back to its darker colour if it hasnt.
		//If rawX and oldX are different, then this would indicated that the touchscreen  is still in use.  If they are
		//not, then this would indicate that the touchsreen is  not in use.
		if ( rawX != oldX){
			timer = mymillis();	//Reset timer if touchscreen is in use
			oldX = rawX;		//Store new X value to check in next loop
			oldY = rawY;		//Store new Y value to check in next loop
		}
		if ((mymillis()-timer > 300)){	//Has 300ms passed since the last timer rest
			rawX = 0;		//Set rawX to 0 will force button colours to reset.
			rawY = 0;
		}

		//Copy our back buffer to the TFT
		memcpy (fbp, backBufferP, var.xres * var.yres * 2);

		//Work out the values needed to control the servos.
		//panValue and tiltValue will be a percentage of the slide
		float panValue = (float)button[0].delta/ (button[0].coords[L]+button[0].coords[W]);
		float tiltValue = (float)button[1].delta /(button[1].coords[L]+button[1].coords[H]);

		//Control servos
		pantilt(panValue,tiltValue);

		printf (" time %i \n", mymillis() - time);
	}

}



void videoCapture()
{
	char str[100];
	char timeAndDate[100];
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	//Create a string with the current time and date. This will be used to append to the file name
	strftime(timeAndDate, sizeof(timeAndDate)-1, "%Y%m%d%H%M%S", t);
	sprintf(str, "video-%s.h264", timeAndDate);

	//Parameter list used in raspivid
	char  *parmList[] = {"/opt/vc/bin/raspivid","-v","-t", "0","-rot","180","-h","720","-w","1280","-o", str, NULL};

	//Create child process that does the recording
	if (pid == 0)
		if ((pid = fork()) == -1)
        		perror("fork error");
		else if (pid == 0)
			execv("/opt/vc/bin/raspivid", parmList);
}


void drawButton(int x, int y, int w, int h, char *text, int backgroundColor, int foregroundColor)

{
        char *p = text;
        int length = 0;

        //get length of the string *text
        while(*(p+length))
                length++;

        //
        if((length*8)> (w-2)){
                printf("####error,button too small for text####\n");
                exit(1);
        }

        //Draw button outline
        drawSquare(x-2,y-2,w+4,h+4,backgroundColor);

        //Draw button foreground
        drawSquare(x,y,w,h,foregroundColor);
        //Place text on the button[0].  Try and center it in a primitive way
        put_string(x+((w-(length*8))/2), y+((h-8)/2),text,4);
}

int mymillis()
{
	//Timer function
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec) * 1000 + (tv.tv_usec)/1000;
}


void  INThandler(int sig)
{
	//Catch signal or "Ctrl-C" and close out used services
        signal(sig, SIG_IGN);
        closeFramebuffer();
        closeFbcp();
        closeServos();
        if (pid != 0){
		kill(pid,SIGKILL);
                pid = 0;
	}
	exit(0);
}

