# Contributing to the Time Circuits Replica Project

First off, thank you for considering contributing! This project is a labor of love, and every contribution, from a small typo fix to a major new feature, is greatly appreciated. These guidelines will help you get started.

## Code of Conduct

This project and everyone participating in it is governed by our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior.

## How Can I Contribute?

There are many ways to contribute to the project:

*   **Reporting Bugs**: If you find a bug, please check the existing [issues](https://github.com/rananna/back-to-the-future-timecircuits/issues) to see if it has already been reported. If not, [open a new issue](https://github.com/rananna/back-to-the-future-timecircuits/issues/new?template=bug_report.md) with a clear title and a detailed description.
*   **Suggesting Enhancements**: If you have an idea for a new feature, please [open a new issue](https://github.com/rananna/back-to-the-future-timecircuits/issues/new?template=feature_request.md) to start a discussion. This allows us to coordinate our efforts and prevent duplication of work.
*   **Improving Documentation**: If you find any part of the documentation to be unclear or incomplete, please feel free to submit a pull request with your improvements.
*   **Submitting Pull Requests**: If you're ready to contribute code, you can submit a pull request.

## Your First Code Contribution

Unsure where to begin contributing? You can start by looking through `good first issue` and `help wanted` issues:
*   [Good first issues](https://github.com/rananna/back-to-the-future-timecircuits/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) - issues which should only require a few lines of code, and a test or two.
*   [Help wanted issues](https://github.com/rananna/back-to-the-future-timecircuits/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) - issues which should be a bit more involved than `good first issue` issues.

## Setting Up Your Development Environment

For instructions on how to set up your development environment, including hardware requirements and firmware installation, please see the **[Developer Guide](docs/developer/developer-guide.md)**. This guide provides a comprehensive overview of the project's architecture and development workflow.

## Pull Request Process

1.  **Fork & Branch**: Fork the repository and create a new branch from `main` for your changes.
2.  **Make Your Changes**: Make your changes, adhering to the coding and documentation standards below.
3.  **Update Documentation**: **This is important.** If your changes affect user-facing features, APIs, or the project's architecture, update the relevant documentation. See the Documentation Guidelines below.
4.  **Submit Pull Request**: Ensure your pull request has a clear title and a detailed description of the changes. Reference any relevant issues.

## Coding Standards

*   **Style**: Please adhere to the existing code style. The project uses a style based on the Google C++ Style Guide.
*   **Comments**: Write clear and concise comments to explain complex logic.
*   **Commit Messages**: Write clear and descriptive commit messages. The first line should be a short summary (50 characters or less), followed by a a more detailed explanation if necessary.

## Documentation Guidelines

Keeping our documentation in sync with our code is critical. Good documentation empowers users and developers, reduces support questions, and makes the project easier to contribute to.

-   **Update Docs with Code**: All pull requests that introduce or change a feature, API, or user-facing behavior **must** include corresponding documentation updates in the same PR.
-   **Where to Document**: Our documentation is centralized to make it easy to find information. Please adhere to the following structure:
    -   **User-Facing Features**: All guides for end-users (e.g., new settings, UI changes, Home Assistant features) belong in the `docs/user-guide/` directory.
    -   **Developer & API Changes**: All technical documentation, including architectural changes, new sequencer commands, or MQTT API modifications, should be added to the consolidated **[`Developer & Sequencer API Guide`](docs/developer/developer-guide.md)**.
    -   **Central Hub**: If you add a new document, be sure to add a link to it in the main documentation hub file: **[`docs/README.md`](docs/README.md)**.
    -   **Main `README.md`**: The root `README.md` should only be updated for major, high-level changes.
-   **Clarity and Conciseness**: Write clearly and simply. Avoid jargon where possible.
-   **Check for Broken Links**: Please ensure that all links in the documentation are up-to-date and working correctly.

Thank you again for your interest in contributing!