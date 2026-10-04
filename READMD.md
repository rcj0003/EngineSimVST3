# What is this?



EngineSimVST3 is a VST that integrates AngeTheGreat's Engine Simulator Demo into your DAW. The goal of this project was simple: figure out how to integrate Engine Simulator into my DAW so that I could automate the parameters.



![Alt text](docs/public/screenshots/screenshot_daw_integration.png?raw=true)



Outside of the engine (which is not able to be changed via automation but can be changed otherwise), all of the parameters displayed above are available to have automations created for them.


# Disclaimers

1. Engine Simulator is already taxing on your CPU as a standalone application. It's simulating the physics of an engine to produce the sound, after all. As a VST? Perhaps a little more taxing. I wouldn't count on being able to run it in real-time without making some sacrifices. Examples of that might include: freezing or bouncing the track, decreasing simulation frequency, or choosing a different engine that is not as taxing.
2. Engine Simulator was originally designed for compilation with MSVC. Some changes were made to make it platform agnostic and able to be compiled on multiple different platforms.

# Credits


AngeTheGreat and his work on Engine Simulator - I was ecstatic when I learned of the existence of Engine Simulator. I've enjoyed watching the continued development on it