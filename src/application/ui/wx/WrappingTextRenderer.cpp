#include "ui/wx/WrappingTextRenderer.hpp"
#include "util/Logger.hpp"
#include <wx/dcclient.h>
#include <wx/tokenzr.h>

namespace ui::wx
{

WrappingTextRenderer::WrappingTextRenderer(int maxWidth)
    : wxDataViewCustomRenderer(wxT("string"), wxDATAVIEW_CELL_INERT, wxALIGN_LEFT)
    , m_maxWidth(maxWidth)
{
    util::Logger::Debug("WrappingTextRenderer created (maxWidth={})", maxWidth);
}

bool WrappingTextRenderer::SetValue(const wxVariant& value)
{
    m_text = value.GetString();
    m_cachedText = wxEmptyString; // Invalidate cache
    static int logCount = 0;
    if (logCount < 5)
    {
        util::Logger::Debug("WrappingTextRenderer::SetValue sample: {}",
            wxString(m_text).Truncate(80).ToStdString());
        ++logCount;
    }
    return true;
}

bool WrappingTextRenderer::GetValue(wxVariant& value) const
{
    value = m_text;
    return true;
}

bool WrappingTextRenderer::Render(wxRect rect, wxDC* dc, int state)
{
    if (m_text.IsEmpty())
        return true;

    static int renderLogCount = 0;
    if (renderLogCount < 5)
    {
        util::Logger::Debug(
            "WrappingTextRenderer::Render rect({}, {}, {}x{}) text len={}",
            rect.GetX(), rect.GetY(), rect.GetWidth(), rect.GetHeight(),
            m_text.Length());
        ++renderLogCount;
    }

    if (dc)
    {
        m_cachedSize = CalculateTextSize(m_text, dc);
        m_cachedText = m_text;
    }

    DrawWrappedText(dc, rect, m_text, state);
    return true;
}

wxSize WrappingTextRenderer::GetSize() const
{
    if (m_text.IsEmpty())
        return wxSize(0, 0);

    if (m_cachedText == m_text && m_cachedSize.GetWidth() > 0)
        return m_cachedSize;

    wxSize fallback(m_maxWidth, 20);
    static int sizeLogCount = 0;
    if (sizeLogCount < 5)
    {
        util::Logger::Debug(
            "WrappingTextRenderer::GetSize fallback {}x{} for text len={}",
            fallback.GetWidth(), fallback.GetHeight(), m_text.Length());
        ++sizeLogCount;
    }
    wxDataViewColumn* column = GetOwner();
    wxDataViewCtrl* view = column ? column->GetOwner() : nullptr;
    if (view)
    {
        wxClientDC dc(view);
        m_cachedSize = CalculateTextSize(m_text, &dc);
        m_cachedText = m_text;
        return m_cachedSize;
    }

    return fallback;
}

void WrappingTextRenderer::SetMaxWidth(int maxWidth)
{
    if (m_maxWidth != maxWidth)
    {
        util::Logger::Debug("WrappingTextRenderer max width {} -> {}", m_maxWidth, maxWidth);
        m_maxWidth = maxWidth;
        m_cachedSize = wxSize();
        m_cachedText.clear();
    }
}

int WrappingTextRenderer::GetMaxWidth() const
{
    return m_maxWidth;
}

wxSize WrappingTextRenderer::CalculateTextSize(const wxString& text, wxDC* dc) const
{
    if (text.IsEmpty() || !dc)
        return wxSize(0, 0);

    wxStringList lines = WrapTextToLines(text, dc, m_maxWidth);
    
    int maxWidth = 0;
    int totalHeight = 0;
    int lineHeight = dc->GetCharHeight();
    
    for (const wxString& line : lines)
    {
        wxSize lineSize = dc->GetTextExtent(line);
        maxWidth = wxMax(maxWidth, lineSize.GetWidth());
        totalHeight += lineHeight;
    }

    // Add small padding to avoid clipped glyphs
    return wxSize(maxWidth + 4, totalHeight + 4);
}

void WrappingTextRenderer::DrawWrappedText(wxDC* dc, const wxRect& rect, const wxString& text, int state)
{
    if (text.IsEmpty() || !dc)
        return;

    // Wrap text to lines
    wxStringList lines = WrapTextToLines(text, dc, wxMax(1, rect.GetWidth() - 4));
    
    int lineHeight = dc->GetCharHeight();
    int y = rect.GetY() + 2; // Small top margin
    
    static int drawLogCount = 0;
    for (const wxString& line : lines)
    {
        if (y + lineHeight > rect.GetBottom())
            break; // Don't draw outside the rect
            
        wxRect lineRect = rect;
        lineRect.y = y;
        lineRect.height = lineHeight;
        RenderText(line, 2, lineRect, dc, state);
        if (drawLogCount < 5)
        {
            util::Logger::Debug("WrappingTextRenderer drawing line: {}",
                wxString(line).Truncate(80).ToStdString());
            ++drawLogCount;
        }
        y += lineHeight;
    }
}

wxStringList WrappingTextRenderer::WrapTextToLines(const wxString& text, wxDC* dc, int maxWidth) const
{
    wxStringList lines;
    auto addLine = [&lines](const wxString& value)
    {
        lines.Add(value.wx_str());
    };
    
    if (text.IsEmpty() || !dc || maxWidth <= 0)
    {
        addLine(text);
        return lines;
    }

    // First split by existing newlines
    wxStringTokenizer tokenizer(text, wxT("\n"), wxTOKEN_RET_EMPTY_ALL);
    
    while (tokenizer.HasMoreTokens())
    {
        wxString line = tokenizer.GetNextToken();
        
        // If line fits, add it as is
        if (dc->GetTextExtent(line).GetWidth() <= maxWidth)
        {
            addLine(line);
            continue;
        }
        
        // Line is too long, need to wrap
        wxString currentLine;
        wxStringTokenizer wordTokenizer(line, wxT(" \t"), wxTOKEN_RET_EMPTY_ALL);
        
        while (wordTokenizer.HasMoreTokens())
        {
            wxString word = wordTokenizer.GetNextToken();
            wxString testLine = currentLine.IsEmpty() ? word : currentLine + wxT(" ") + word;
            
            if (dc->GetTextExtent(testLine).GetWidth() <= maxWidth)
            {
                currentLine = testLine;
            }
            else
            {
                // Current word doesn't fit, start new line
                if (!currentLine.IsEmpty())
                {
                    addLine(currentLine);
                    currentLine = word;
                }
                else
                {
                    // Even single word doesn't fit, try to break it
                    if (dc->GetTextExtent(word).GetWidth() > maxWidth)
                    {
                        // Break long word
                        for (size_t i = 0; i < word.length(); )
                        {
                            wxString part;
                            for (size_t j = i; j < word.length(); ++j)
                            {
                                wxString testPart = word.Mid(i, j - i + 1);
                                if (dc->GetTextExtent(testPart).GetWidth() > maxWidth && !part.IsEmpty())
                                    break;
                                part = testPart;
                            }
                            if (part.IsEmpty())
                                part = word.Mid(i, 1); // At least one character
                            addLine(part);
                            i += part.length();
                        }
                    }
                    else
                    {
                        addLine(word);
                    }
                }
            }
        }
        
        if (!currentLine.IsEmpty())
            addLine(currentLine);
    }
    
    if (lines.IsEmpty())
        addLine(text); // Fallback
        
    return lines;
}

} // namespace ui::wx