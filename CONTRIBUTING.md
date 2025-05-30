# Contributing

This project welcomes contributions and suggestions. Most contributions require you to
agree to a Contributor License Agreement (CLA) declaring that you have the right to,
and actually do, grant us the rights to use your contribution. For details, visit
https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need
to provide a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the
instructions provided by the bot. You will only need to do this once across all repositories using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/)
or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

# Contribution Guidelines

We welcome contributions from everyone. The following guidelines will help you understand our expectations and streamline the review process.

## Branch
All PRs should be made against the `dev` branch. Please ensure your branch is up to date with the latest changes before submitting a pull request. And the `main` branch is used for release. Consider `main` as the stable branch and `dev` as the development branch.

## Code Style

We follow the LLVM coding style for all C/C++ code. Please ensure your code conforms to this style. To automate this process, we use `clang-format`:

```sh
clang-format -i path/to/your/file.cpp
```

It is recommended to install the [Clang-Format extension](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format) in Visual Studio Code to format your code automatically.

## Commit Messages

Please follow these guidelines for commit messages:

- Use the present tense ("Add feature" not "Added feature").
- Use the imperative mood ("Move cursor to..." not "Moves cursor to...").
- Limit the first line to 72 characters or less.
- Reference issues and pull requests liberally.

## Pre-commit Hooks

We use pre-commit hooks to ensure code quality and consistency. These hooks will check your code for proper formatting before allowing a commit.

### Setting up pre-commit hooks for the first time

1. Install the pre-commit package, for example, in Ubuntu you can run:
    ```sh
    sudo apt install pre-commit
    ```

2. Install the pre-commit hooks:
    ```sh
    pre-commit install --hook-type commit-msg
    pre-commit install
    ```

3. Manually run the hooks on all files (optional but recommended for first-time setup):
    ```sh
    pre-commit run --all-files
    ```

4. Auto Format when you commit:
    ```sh
    export AUTO_FORMAT=1
    ```

5. Install cppcheck:
    ```sh
    sudo apt-get install cppcheck
    ```

6. Disable cppcheck:
    ```sh
    export DISABLE_CPPCHECK=1
    ```

## Submitting a Pull Request

1. Ensure your code follows the style guide and passes all tests.
2. Commit your changes with a descriptive commit message.
3. Push your changes to your fork:
    ```sh
    git push origin your-branch
    ```
4. Create a pull request against the `main` branch of the upstream repository.

In your pull request, please include:
- A descriptive title and detailed description of the changes.
- Reference any relevant issues or pull requests.

## Additional Resources

- [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html)
- [Clang-Format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [GitHub Guides: Forking Projects](https://guides.github.com/activities/forking/)
- [Pre-commit Documentation](https://pre-commit.com/)