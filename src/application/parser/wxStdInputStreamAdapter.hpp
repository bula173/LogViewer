#include <wx/wx.h>
#include <wx/stream.h>
#include <istream>

// Custom wrapper for std::istream
class wxStdInputStreamAdapter : public wxInputStream
{
private:
  std::istream &m_stream;
  std::streampos m_totalSize;

public:
  wxStdInputStreamAdapter(std::istream &stream) : m_stream(stream)
  {
    // Save the current position
    std::streampos currentPos = m_stream.tellg();

    // Seek to the end to get the total size
    m_stream.seekg(0, std::ios::end);
    m_totalSize = m_stream.tellg();

    // Seek back to the original position
    m_stream.seekg(currentPos);
  }

  size_t OnSysRead(void *buffer, size_t size) override
  {
    m_stream.read(static_cast<char *>(buffer), size);
    return m_stream.gcount(); // Return the number of bytes read
  }

  bool CanRead() const override
  {
    return m_stream.good();
  }

  bool Eof() const override
  {
    return m_stream.eof();
  }

  double GetReadPercentage() const
  {
    if (m_totalSize == 0)
    {
      return 0.0;
    }

    std::streampos currentPos = m_stream.tellg();
    return static_cast<double>(currentPos) / static_cast<double>(m_totalSize) * 100.0;
  }

  double GetTotalSize() const
  {
    return m_totalSize;
  }
};