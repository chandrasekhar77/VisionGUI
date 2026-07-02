#pragma once

// Owned by L2 (Sequence Controller). Implemented by the model repository.
// Used by L4 (ModelManagerPane, RecipeView) and L2 (SequenceController).
// All methods called on UI thread — not required to be thread-safe.
class IModelLoader
{
public:
    virtual ~IModelLoader() = default;

    // Returns list of model names available on disk (display names, not file paths).
    virtual void    GetModelList(CStringArray& outNames) = 0;

    // Load the named model as the active model. Returns false on failure.
    virtual bool    LoadModel(LPCTSTR name) = 0;

    // Name of the currently loaded model, or empty string if none.
    virtual CString ActiveModelName() const = 0;

    // Create a new empty model with the given name. Returns false if name exists.
    virtual bool    NewModel(LPCTSTR name) = 0;

    // Delete a model from disk. Returns false if it is the active model.
    virtual bool    DeleteModel(LPCTSTR name) = 0;

    // Duplicate srcName → dstName. Returns false if dstName already exists.
    virtual bool    DuplicateModel(LPCTSTR srcName, LPCTSTR dstName) = 0;
};
