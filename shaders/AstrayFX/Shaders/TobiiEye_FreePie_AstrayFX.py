#To use this script load it up in FreePie and make sure you have Tobii Eye Tracker fully installed and Enabled.

#Then make sure to start FreePie in Admin mode then run this script. Once that is done Move your head the full
#length of your monitor so that it calibrates it's center. Then start your game with reshade and start my Shader.
#Once in game and in SuperDepth3D's Menu use the Calibrate slider to reduce the ghosting in game to a tolerable
#level. There should be a veary slight delay in tracking this is ok for now maybe it can be improved later.

#Thank you and as always for more information head over to http://www.Depth3D.com
#If you like to dontate please do at. https://www.buymeacoffee.com/BlueSkyDefender

#Downloads Needed
#https://andersmalmgren.github.io/FreePIE/
#https://gaming.tobii.com/getstarted/

#If you don't know what your doing don't adjust the script below this line.
def map_tobii(n):
	IsLooking = tobiiEyeX.userPresence == "Present"
	Center = 4
	MC = Center * 2

	if (IsLooking): #and enabled):
		#yaw = rotobiiEyeX.yaw
		#roll = tobiiEyeX.roll
		#x = tobiiEyeX.averageEyePositionInMmX
		#y = tobiiEyeX.averageEyePositionInMmY
		#z = tobiiEyeX.averageEyePositionInMmZ
		x = tobiiEyeX.averageEyePositionNormalizedX * MC - Center
		y = tobiiEyeX.averageEyePositionNormalizedY * MC - Center
		z = round(tobiiEyeX.averageEyePositionNormalizedZ * MC)
		#x = tobiiEyeX.normalizedCenterDeltaX
		#y = tobiiEyeX.normalizedCenterDeltaY
		#x = tobiiEyeX.gazePointInPixelsX
		#y = tobiiEyeX.gazePointInPixelsY
   	else :
		#yaw = 0
		#roll = 0
		x = 0
		y = 0
		z = 0

	#freePieIO[0].yaw = 0
	#freePieIO[0].pitch = 0
	#freePieIO[0].roll = 0
	freePieIO[0].x = x
	freePieIO[0].y = y
	freePieIO[0].z = z

	#diagnostics.watch(enabled)
	#diagnostics.watch(yaw)
	#diagnostics.watch(roll)
   	diagnostics.watch(x)
	diagnostics.watch(y)
	diagnostics.watch(z)

#Tobii update function
def update():
   map_tobii(0)

if starting:
	#enabled = True
	system.setThreadTiming(TimingTypes.HighresSystemTimer)
	system.threadExecutionInterval = 0
	tobiiEyeX.update += update

#toggleA = keyboard.getPressed(Key.X)

#if toggleA:
	#enabled = not enabled
