/**********************************************************
Scanner circuit & its functions
*********************************************************/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef CIRCUIT
#include "circuit.h"
#endif

#ifndef CORESCANNER
#include "core_scanner.h"
#endif

double* ScannerParams(int index) {
	return circuits[index].params;
}

void Scanner_DoIdle( circuit *c ) {
    //this is the default scanner update function
    //...
    //which does nothing!
}

int Scanner( int owner ) {
	
    circuit c = NewCircuit();
    c.nI = 0;
    c.nO = 4;
    
    c.plen = 11;
    c.params = (double*)calloc(c.plen,sizeof(double));
	
    c.iplen = 6;
    c.iparams = (int*)calloc(c.iplen,sizeof(int));
   
    c.updatef = Scanner_DoIdle; //this is the default scanner update function
    
    int index = AddToCircuits(c,owner);
    printf("cCore: Scanner Initialised\n");
    return index;

}
/****************************************
 * params[0-2] = x,y,z
 * params[3-5] = target x,y,z
 * params[6] = velocity - deprecated
 * params[7-9] = movement step sizes x,y,z
 * 
 * iparams[0] = steps required to complete action
 * iparams[1] = elapsed steps
 * iparams[2] = fast scan resolution - points per line
 * iparams[3] = slow scan resolution - lines
 * iparams[4] = number of steps to wait before activating record
 * iparams[5] = elapsed steps since the last record event
 * 
 * *************************************/

int Scanner_Move(int index, double x, double y, double z, double v) {
	
    circuits[index].updatef = Scanner_DoMove;
    //circuit c = circuits[index]; //this is very unsafe!

    // target x
    circuits[index].params[3] = circuits[index].params[0] + x;
    circuits[index].params[4] = circuits[index].params[1] + y;
    circuits[index].params[5] = circuits[index].params[2] + z;
    circuits[index].params[6] = v; //velocity

    // time duration
    double length = x*x + y*y + z*z;
    length = sqrt(length);
    
    double timeneeded = length/v;
    int steps = (int)ceil(timeneeded/dt);
    circuits[index].iparams[0] = steps;
    circuits[index].iparams[1] = 0;
    

    //saves the initial position before moving
    circuits[index].params[7] = circuits[index].params[0];
    circuits[index].params[8] = circuits[index].params[1];
    circuits[index].params[9] = circuits[index].params[2];
    
    return steps;
}
void Scanner_DoMove( circuit *c ) {
	
	c->iparams[1]++; //increment the elapsed time
	
    // pos = pos + step
    c->params[0] = c->params[7] + c->iparams[1]*(c->params[3]-c->params[7])/c->iparams[0];
    c->params[1] = c->params[8] + c->iparams[1]*(c->params[4]-c->params[8])/c->iparams[0];
    c->params[2] = c->params[9] + c->iparams[1]*(c->params[5]-c->params[9])/c->iparams[0];
       
    // at the end adjust all the values to the target
    if (c->iparams[0] == c->iparams[1]) {
		
		c->params[0] = c->params[3];
		c->params[1] = c->params[4];
		c->params[2] = c->params[5];
		c->updatef = Scanner_DoIdle; //set idle mode
	}
    
    //OUTPUT VALUES
    GlobalBuffers[c->outputs[0]] = c->params[0];
    GlobalBuffers[c->outputs[1]] = c->params[1];
    GlobalBuffers[c->outputs[2]] = c->params[2];
    

    //printf("%f %i %i \n",c->params[0],c->iparams[0],c->iparams[1]);


}

int Scanner_Move_Record(int index, double x, double y, double z, double v, int npts) {
	
	//setup a normal move command
    int steps = Scanner_Move(index,x,y,z,v);
    
    //... but use a different update function
    circuits[index].updatef = Scanner_DoMove_RecordF; //TODO: different for Backward scan!
    
    //nuber of steps to wait between record events
    int dL = (int)(floor((float)steps/(float)npts));
    circuits[index].iparams[4] = dL;
    circuits[index].iparams[5] = 0;
    
    //record the first point
    GlobalBuffers[circuits[index].outputs[3]] = 1;
    
    //return the number of steps to wait for
    return steps;
}

void Scanner_DoMove_RecordF( circuit *c ) {
	
	c->iparams[5]++; //increment the elapsed time between record events
	
	Scanner_DoMove(c); //do move normally
	
	//if we reached the point where we should record...
	if(c->iparams[5] == c->iparams[4] || c->iparams[1] == 1) {
		printf(".");
		GlobalBuffers[c->outputs[3]] = 1;
		c->iparams[5] = 0;
	} else {
		GlobalBuffers[c->outputs[3]] = 0;
	}
	if (c->iparams[0] == c->iparams[1]) {
		GlobalBuffers[c->outputs[3]] = 0;
		printf("\n");
	}
	
}


int Scanner_Place (int index, double x,double y, double z) {
    circuits[index].updatef = Scanner_DoPlace;
    circuit c = circuits[index];
    
    // target x
    c.params[0] = x;
    c.params[1] = y;
    c.params[2] = z;

    return 0;
}
void Scanner_DoPlace( circuit *c ) {
    GlobalBuffers[c->outputs[0]] = c->params[0];
    GlobalBuffers[c->outputs[1]] = c->params[1];
    GlobalBuffers[c->outputs[2]] = c->params[2];
   // printf("%f %f %f \n",c->params[0],c->params[1],c->params[2]);

}

int Scanner_MoveTo (int index, double x,double y, double z, double v) {
    
	//circuits[index].updatef = Scanner_DoMoveTo;
	
	//calculate the change
	double dx = x-circuits[index].params[0];
	double dy = y-circuits[index].params[1];
	double dz = z-circuits[index].params[2];

	int steps = Scanner_Move(index, dx, dy, dz, v);

	return steps;
}

void Scanner_DoMoveTo( circuit *c ) { //not needed any more
    // pos = pos + step*direction
    c->params[0] = c->params[0] + c->params[6]*dt*c->params[7];
    c->params[1] = c->params[1] + c->params[6]*dt*c->params[8];
    c->params[2] = c->params[2] + c->params[6]*dt*c->params[9];

    // if increasing then use these 3 if statements
    if (c->params[7]==1){
    if ((c->params[0]) >= (c->params[3]) ) {c->params[0] = c->params[3];}
    }

    if (c->params[8]==1){
    if ((c->params[1]) >= (c->params[4]) ) {c->params[1] = c->params[4];}
    }


    if (c->params[9]==1){
    if ((c->params[2]) >= (c->params[5]) ) {c->params[2] = c->params[5];}
    }


    // if decreasing use these if statements
    if (c->params[7]==-1){
    if ((c->params[0]) <= (c->params[3]) ) {c->params[0] = c->params[3];}
    }

    if (c->params[8]==-1){
    if ((c->params[1]) <= (c->params[4]) ) {c->params[1] = c->params[4];}
    }


    if (c->params[9]==-1){
    if ((c->params[2]) <= (c->params[5]) ) {c->params[2] = c->params[5];}
    }






    c->iparams[1] = c->iparams[1] + 1;
    // at the end adjust all the values
    if (c->iparams[0] == c->iparams[1])
        {
            c->params[0] = c->params[3];
            c->params[1] = c->params[4];
            c->params[2] = c->params[5];
        }
    // idle for the time left
    if (c->iparams[0] == c->iparams[1]) {c->updatef = Scanner_DoIdle;}
    
    //OUTPUT VALUES
    GlobalBuffers[c->outputs[0]] = c->params[0];
    GlobalBuffers[c->outputs[1]] = c->params[1];
    GlobalBuffers[c->outputs[2]] = c->params[2];
    

    //printf("%f %f %f \n",c->params[0],c->params[1],c->params[2]);


}



int Scanner_Scan (int index, double x,double y, double z, double v, int points) {
	
    circuits[index].updatef = Scanner_DoScan;
    circuit c = circuits[index];
    
    double changex = x-c.params[0];
    double changey = y-c.params[1];
    double changez = z-c.params[2];

    // target x
    c.params[3] = x;
    c.params[4] = y;
    c.params[5] = z;

    c.params[6] = v;

    // direction
    c.params[7] = 1;
    c.params[8] = 1;
    c.params[9] = 1;
    // check if -ve direction
    if (c.params[0] > c.params[3]){c.params[7] = -1;}
    if (c.params[1] > c.params[4]){c.params[8] = -1;}
    if (c.params[2] > c.params[5]){c.params[9] = -1;}



    // find largest step size
    double stepx = abs(floor( (changex)/ (dt*v) ));
    double stepy = abs(floor( (changey)/ (dt*v) ));
    double stepz = abs(floor( (changez)/ (dt*v) ));

    double steps = stepx;
    if (stepy > steps) {steps = stepy;}
    if (stepz > steps) {steps = stepz;}

    c.iparams[0]=steps;
    c.iparams[1]=0;
    c.iparams[2] = points;
    return steps;
}


void Scanner_DoScan( circuit *c )
{
    int Record = 0;
    // pos = pos + step*direction
    c->params[0] = c->params[0] + c->params[6]*dt*c->params[7];
    c->params[1] = c->params[1] + c->params[6]*dt*c->params[8];
    c->params[2] = c->params[2] + c->params[6]*dt*c->params[9];

    // if increasing then use these 3 if statements
    if (c->params[7]==1){
    if ((c->params[0]) >= (c->params[3]) ) {c->params[0] = c->params[3];}
    }

    if (c->params[8]==1){
    if ((c->params[1]) >= (c->params[4]) ) {c->params[1] = c->params[4];}
    }


    if (c->params[9]==1){
    if ((c->params[2]) >= (c->params[5]) ) {c->params[2] = c->params[5];}
    }


    // if decreasing use these if statements
    if (c->params[7]==-1){
    if ((c->params[0]) <= (c->params[3]) ) {c->params[0] = c->params[3];}
    }

    if (c->params[8]==-1){
    if ((c->params[1]) <= (c->params[4]) ) {c->params[1] = c->params[4];}
    }


    if (c->params[9]==-1){
    if ((c->params[2]) <= (c->params[5]) ) {c->params[2] = c->params[5];}
    }


    c->iparams[1] = c->iparams[1] + 1;
    // at the end adjust all the values
    if (c->iparams[0] == c->iparams[1])
        {
            c->params[0] = c->params[3];
            c->params[1] = c->params[4];
            c->params[2] = c->params[5];
        }
    // idle for the time left
    if (c->iparams[0] == c->iparams[1]) {c->updatef = Scanner_DoIdle;}
    if ( (c->iparams[1]%c->iparams[2]) == 0) {Record = 1;}
    
    //OUTPUT VALUES
    GlobalBuffers[c->outputs[0]] = c->params[0];
    GlobalBuffers[c->outputs[1]] = c->params[1];
    GlobalBuffers[c->outputs[2]] = c->params[2];
    GlobalBuffers[c->outputs[3]] = Record;
    

   // printf("%f %f %f %i \n",c->params[0],c->params[1],c->params[2], Record);


}
