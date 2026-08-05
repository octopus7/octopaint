#include <octopaint/application/Workspace.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
    void Require(bool const condition, char const* const message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

int main()
{
    try
    {
        octopaint::application::Workspace workspace;

        auto snapshot = workspace.Snapshot();
        Require(!snapshot.has_document, "A new workspace must be empty.");
        Require(snapshot.status_message == "Ready", "An empty workspace must be ready.");

        workspace.NewDocument("Test document", { .width = 800, .height = 600 });
        snapshot = workspace.Snapshot();

        Require(snapshot.has_document, "NewDocument must create an active document.");
        Require(snapshot.document_title == "Test document", "The document title must cross the application boundary.");
        Require(snapshot.canvas_size.width == 800 && snapshot.canvas_size.height == 600, "Canvas dimensions must be preserved.");
        Require(snapshot.status_message == "800 x 600 pixels", "The application must provide presentation-ready status text.");

        std::cout << "OctoPaint core tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint core tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

