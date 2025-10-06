# Contributing to the Time Circuits Replica Project

First off, thank you for considering contributing! This project is a labor of love, and every contribution, from a small typo fix to a major new feature, is greatly appreciated.

This document provides some guidelines for contributing to the project.

## How Can I Contribute?

*   **Reporting Bugs**: If you find a bug, please open an issue on GitHub. Include as much detail as possible, such as the steps to reproduce the bug, the expected behavior, and the actual behavior.
*   **Suggesting Enhancements**: If you have an idea for a new feature or an improvement to an existing one, please open an issue to start a discussion.
*   **Pull Requests**: If you've fixed a bug or implemented a new feature, you can submit a pull request.

## Submitting a Pull Request

1.  Fork the repository and create your branch from `main`.
2.  Make your changes.
3.  Ensure your code follows the existing style and conventions.
4.  If you've added a new user-facing feature, please update the relevant documentation in the `docs/` directory.
5.  Submit your pull request with a clear description of the changes you've made.

## Adding New Sequencer Commands & Animations

The command sequencer is a core part of this project and a great place to contribute.

### Adding a New Sequencer Command

1.  **Define the Command Enum**: Add a new entry to the `SequenceCommand` enum in `AnimationManager.h`.
2.  **Implement the Logic**: Add a new `case` to the `switch` statement in the `handleSequencer` function in `AnimationManager.cpp`. This is where you'll implement the logic for your new command.
3.  **Document the Command**: Add a new row to the command reference table in `docs/developer/sequencer-api.md`.

### Adding a New Built-in Animation

1.  **Create a Generator Function**: In `AnimationSequences.cpp`, create a new function (e.g., `generateMyCoolAnimation(SequencerTrack tracks[3])`) that uses the `add_step` helper to build your animation sequence across the three tracks.
2.  **Define the AnimationType**: Add a new entry to the `AnimationType` enum in `AnimationSequences.h`.
3.  **Register the Animation**: Add a new `case` to the `switch` statement in the `generateAnimationSequence` function in `AnimationSequences.cpp` that calls your new generator function.
4.  **Add to `Randomize All`**: If your animation is suitable for general use, consider adding its `AnimationType` to the `validAnimationStyles` array in `generateAnimationSequence` so it can be triggered by the "Randomize All" feature.
5.  **Document the Animation**: Add a new row to the "Available Animations" table in `docs/developer/sequencer-api.md`.

Thank you again for your interest in contributing!