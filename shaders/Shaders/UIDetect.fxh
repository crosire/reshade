//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//UIDetect header file
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/*
Description:

PIXELNUMBER                     //total number of pixels used for UI detection
UIPixelCoord[PIXELNUMBER]       //the UI pixels screen space coordinates (top left is 0,0) and UI number;
{
    float3(x1,y1,UI1),
    float3(x2,y2,UI1),
    float3(x3,y3,UI1),
    float3(x4,y4,UI2),
    float3(x5,y5,UI3),
    ...
}
UIPixelRGB[PIXELNUMBER]         //the UI pixels RGB values
{
    float3(Red1,Green1,Blue1),
    float3(Red2,Green2,Blue2),
    ...
}
*/

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


//Game: COD4:MW
//Resolution: 1920x1080

#define PIXELNUMBER 8

static const float3 UIPixelCoord[PIXELNUMBER]=
{
	float3(562,121,1),  //TAB - Mission details
    float3(614,121,1),
    float3(1589,106,2), //ESC - Menu
    float3(1695,106,2),
    float3(272,40,3),   //Options, Controls
    float3(272,33,3),
    float3(1238,174,4), //Main Menu
    float3(1273,174,4),
};

static const float3 UIPixelRGB[PIXELNUMBER]=
{
	float3(255,204,102),
    float3(255,204,102),
    float3(255,204,102),
    float3(255,204,102),
    float3(255,204,102),
    float3(255,204,102),
    float3(255,255,250),
    float3(255,255,249),
};