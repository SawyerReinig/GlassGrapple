# Glass Grapple

Glass Grapple On Github: https://github.com/SawyerReinig/GlassGrapple


This is my OpenGL ES VR game about swinging through holes in walls. 

Graphically I think the things that are most impressive are the fresnel rainbow shader used to hightlight the next hole in the walls. 

The use of the depth test to draw things through walls. 

Everything being built in GLES, so I made the grid walls by myself. 

I used a program called dr_mp3 to read an Mp3 into memory, which can then be played using the Oboe sound system, which is built off AAudio. 

A value valled MusicBrightness is calculated from the average of 10 floating-point PCM samples of the Mp3. This value is then passed to the grid wall shader to make it pulse with the music. 

I also made a basic OBJ loader to read in the grapple that I made in Blender. 

Part of all this was that I had to copy both the song data, and the data for the obj onto the sdk card of the headset. Then these systems could ues fread and fopen as they needed. 

And of course there was lots of vector manipulation to rotate the grapple to be the right orientation in the hand. 