# What is this?



EngineSimVST3 is a VST that integrates AngeTheGreat's Engine Simulator Demo into your DAW. The goal of this project was simple: figure out how to integrate Engine Simulator into my DAW so that I could automate the parameters.



![Alt text](docs/public/screenshots/screenshot_daw_integration.png?raw=true)



Outside of the engine (which is not able to be changed via automation but can be changed otherwise), all of the parameters displayed above are available to have automations created for them.


# Disclaimers

1. Engine Simulator is already taxing on your CPU as a standalone application. It's simulating the physics of an engine to produce the sound, after all. As a VST? Perhaps a little more taxing. I wouldn't count on being able to run it in real-time without making some sacrifices. Examples of that might include: freezing or bouncing the track, decreasing simulation frequency, or choosing a different engine that is not as taxing.
2. Engine Simulator was originally designed for compilation with MSVC. Some changes were made to make it platform agnostic and able to be compiled on multiple different platforms.


# How to use


1. Add VST/AU to MIDI track
2. In the current implementation, there must be a MIDI note that is currently active in order for the simulation to run (instead of it running 24/7 in the initial implementation). If there isn't a MIDI note playing, then the simulation PAUSES (not terminates); playing a MIDI note will resume exactly from where it was last.


# Potential TODOs


- Improve the UI
- Add automation to reset current state of simulation



# Credits

AngeTheGreat and his work on Engine Simulator - I was ecstatic when I learned of the existence of Engine Simulator. I've enjoyed watching the continued development on it.


[Click here](https://github.com/Engine-Simulator/engine-sim-community-edition) to see the currently maintained version of Engine Simulator.


[Click here](ENGINE_SIMULATOR_README.md) to see the original README for this project.
