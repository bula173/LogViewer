#ifndef GUI_WRAPPINGTEXTRENDERER_HPP
#define GUI_WRAPPINGTEXTRENDERER_HPP

#include <wx/dataview.h>
#include <wx/dc.h>

namespace ui::wx
{

/**
 * @brief Custom renderer that wraps long text within wxDataViewCtrl cells.
 *
 * The renderer calculates dynamic heights so every cell can display
 * multi-line content without truncation.
 */
class WrappingTextRenderer : public wxDataViewCustomRenderer
{
public:
    WrappingTextRenderer(int maxWidth = 300);
    virtual ~WrappingTextRenderer() = default;

    // wxDataViewCustomRenderer interface
        /** @brief Cache the value provided by the model. */
    bool SetValue(const wxVariant& value) override;
        /** @brief Return the renderer's cached value. */
    bool GetValue(wxVariant& value) const override;
        /** @brief Paint wrapped text inside the provided rectangle. */
    bool Render(wxRect rect, wxDC* dc, int state) override;
        /** @brief Returns the dynamic size calculated for the current text. */
    wxSize GetSize() const override;
    bool HasEditorCtrl() const override { return false; }
    
    // Custom methods
    void SetMaxWidth(int maxWidth);
    int GetMaxWidth() const;

private:
    wxString m_text;
    int m_maxWidth;
    mutable wxSize m_cachedSize;
    mutable wxString m_cachedText;
    
    wxSize CalculateTextSize(const wxString& text, wxDC* dc) const;
    void DrawWrappedText(wxDC* dc, const wxRect& rect, const wxString& text, int state);
    wxStringList WrapTextToLines(const wxString& text, wxDC* dc, int maxWidth) const;
};

} // namespace ui::wx

#endif // GUI_WRAPPINGTEXTRENDERER_HPP