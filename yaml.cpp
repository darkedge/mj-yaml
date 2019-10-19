#include "mj/yaml.hpp"
#include <string.h>
#include <limits.h>

#include <assert.h>

using namespace mj;

#define INITIAL_STACK_SIZE 16
#define INITIAL_QUEUE_SIZE 16
#define INITIAL_STRING_SIZE 16
/*
 * The size of the input raw buffer.
 */
#define INPUT_RAW_BUFFER_SIZE 16384

/*
 * The size of the input buffer.
 *
 * It should be possible to decode the whole raw buffer.
 */
#define INPUT_BUFFER_SIZE (INPUT_RAW_BUFFER_SIZE * 3)

// YamlString

void YamlToken::Delete(YamlParser& parser)
{
  switch (this->type)
  {
  case EYamlTokenType::TagDirective:
    parser.Free(std::get<tag_directive_t>(this->data).handle);
    parser.Free(std::get<tag_directive_t>(this->data).prefix);
    break;

  case EYamlTokenType::Alias:
    parser.Free(std::get<alias_t>(this->data).value);
    break;

  case EYamlTokenType::Anchor:
    parser.Free(std::get<anchor_t>(this->data).value);
    break;

  case EYamlTokenType::Tag:
    parser.Free(std::get<tag_t>(this->data).handle);
    parser.Free(std::get<tag_t>(this->data).suffix);
    break;

  case EYamlTokenType::Scalar:
    parser.Free(std::get<scalar_t>(this->data).value);
    break;

  default:
    break;
  }

  *this = {};
}

#define STRING(string, length)                                                                     \
  {                                                                                                \
    (string), (string) + (length), (string)                                                        \
  }

#define STRING_ASSIGN(value, string, length)                                                       \
  ((value).start = (string), (value).end = (string) + (length), (value).pointer = (string))

bool YamlString::Init(YamlParser& parser, size_t size)
{
  this->start = (uint8_t*)parser.Malloc(size);
  if (this->start)
  {
    this->pointer = this->start;
    this->end     = this->start + (size);
    memset(this->start, 0, (size));
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

void YamlString::Del(YamlParser& parser)
{
  parser.Free(this->start);
  *this = YamlString();
}

// YamlBuffer
bool YamlBuffer::Init(YamlParser& parser, size_t size)
{
  this->start = (decltype(this->start))parser.Malloc(size);
  if (this->start)
  {
    this->last    = this->start;
    this->pointer = this->start;
    this->end     = this->start + (size);
    return true;
  }
  else
  {
    parser.error = EYamlError::Parser;
    return false;
  }
}

void YamlBuffer::Del(YamlParser& parser)
{
  parser.Free(this->start);
  this->start   = nullptr;
  this->pointer = nullptr;
  this->end     = nullptr;
}

// YamlParser

bool YamlParser::ExtendString(YamlString& string)
{
  uint8_t* new_start =
      (uint8_t*)this->Realloc((void*)string.start, (string.end - string.start) * 2);

  if (!new_start)
  {
    return false;
  }

  memset(new_start + (string.end - string.start), 0, string.end - string.start);

  string.pointer = new_start + (string.pointer - string.start);
  string.end     = new_start + (string.end - string.start) * 2;
  string.start   = new_start;

  return true;
}

bool YamlParser::JoinString(YamlString& a, YamlString& b)
{
  if (b.start == b.pointer)
  {
    return true;
  }

  while (a.end - a.pointer <= b.pointer - b.start)
  {
    if (!this->ExtendString(a))
    {
      return false;
    }
  }

  memcpy(a.pointer, b.start, b.pointer - b.start);
  a.pointer += b.pointer - b.start;

  return true;
}

bool YamlString::Extend(YamlParser& parser)
{
  if ((this->pointer + 5 < this->end) || parser.ExtendString(*this))
  {
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

void YamlString::Clear()
{
  this->pointer = this->start;
  memset(this->start, 0, this->end - this->start);
}

bool YamlString::Join(YamlParser& parser, YamlString& string_b)
{
  if (parser.JoinString(*this, string_b))
  {
    string_b.pointer = string_b.start;
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

/*
 * String check operations.
 */
/*
 * Check the current octet in the buffer.
 */
bool YamlString::CheckAt(char octet, size_t offset)
{
  return (this->pointer[offset] == (uint8_t)octet);
}

/*
 * Check if the character at the specified position is an alphabetical
 * character, a digit, '_', or '-'.
 */
bool YamlString::IsAlphaAt(size_t offset)
{
  return ((this->pointer[offset] >= (uint8_t)'0' && this->pointer[offset] <= (uint8_t)'9') ||
          (this->pointer[offset] >= (uint8_t)'A' && this->pointer[offset] <= (uint8_t)'Z') ||
          (this->pointer[offset] >= (uint8_t)'a' && this->pointer[offset] <= (uint8_t)'z') ||
          this->pointer[offset] == '_' || this->pointer[offset] == '-');
}

/*
 * Check if the character at the specified position is a digit.
 */
bool YamlString::IsDigitAt(size_t offset)
{
  return ((this->pointer[offset] >= (uint8_t)'0' && this->pointer[offset] <= (uint8_t)'9'));
}

/*
 * Get the value of a digit.
 */
uint8_t YamlString::AsDigitAt(size_t offset)
{
  return (this->pointer[offset] - (uint8_t)'0');
}

/*
 * Check if the character at the specified position is a hex-digit.
 */
bool YamlString::IsHexAt(size_t offset)
{
  return ((this->pointer[offset] >= (uint8_t)'0' && this->pointer[offset] <= (uint8_t)'9') ||
          (this->pointer[offset] >= (uint8_t)'A' && this->pointer[offset] <= (uint8_t)'F') ||
          (this->pointer[offset] >= (uint8_t)'a' && this->pointer[offset] <= (uint8_t)'f'));
}

/*
 * Get the value of a hex-digit.
 */
uint8_t YamlString::AsHexAt(size_t offset)
{
  return ((this->pointer[offset] >= (uint8_t)'A' && this->pointer[offset] <= (uint8_t)'F')
              ? (this->pointer[offset] - (uint8_t)'A' + 10)
              : (this->pointer[offset] >= (uint8_t)'a' && this->pointer[offset] <= (uint8_t)'f')
                    ? (this->pointer[offset] - (uint8_t)'a' + 10)
                    : (this->pointer[offset] - (uint8_t)'0'));
}

/*
 * Check if the character is ASCII.
 */
bool YamlString::IsAsciiAt(size_t offset)
{
  return (this->pointer[offset] <= (uint8_t)'\x7F');
}

/*
 * Check if the character can be printed unescaped.
 */
bool YamlString::IsPrintableAt(size_t offset)
{
  return ((this->pointer[offset] == 0x0A)   // . == #x0A
          || (this->pointer[offset] >= 0x20 // #x20 <= . <= #x7E
              && this->pointer[offset] <= 0x7E) ||
          (this->pointer[offset] == 0xC2 // #0xA0 <= . <= #xD7FF
           && this->pointer[offset + 1] >= 0xA0) ||
          (this->pointer[offset] > 0xC2 && this->pointer[offset] < 0xED) ||
          (this->pointer[offset] == 0xED && this->pointer[offset + 1] < 0xA0) ||
          (this->pointer[offset] == 0xEE) ||
          (this->pointer[offset] == 0xEF          // #xE000 <= . <= #xFFFD
           && !(this->pointer[offset + 1] == 0xBB // && . != #xFEFF
                && this->pointer[offset + 2] == 0xBF) &&
           !(this->pointer[offset + 1] == 0xBF &&
             (this->pointer[offset + 2] == 0xBE || this->pointer[offset + 2] == 0xBF))));
}

/*
 * Check if the character at the specified position is NUL.
 */
bool YamlString::IsNulAt(size_t offset)
{
  return CheckAt('\0', offset);
}

/*
 * Check if the character at the specified position is BOM.
 */
bool YamlString::IsBomAt(size_t offset)
{
  return (CheckAt('\xEF', offset) && CheckAt('\xBB', offset + 1) &&
          CheckAt('\xBF', offset + 2)) /* BOM (#xFEFF) */;
}

/*
 * Check if the character at the specified position is space.
 */
bool YamlString::IsSpaceAt(size_t offset)
{
  return CheckAt(' ', offset);
}

/*
 * Check if the character at the specified position is tab.
 */
bool YamlString::IsTabAt(size_t offset)
{
  return CheckAt('\t', offset);
}

/*
 * Check if the character at the specified position is blank (space or tab).
 */
bool YamlString::IsBlankAt(size_t offset)
{
  return (IsSpaceAt(offset) || IsTabAt(offset));
}

/*
 * Check if the character at the specified position is a line break.
 */
bool YamlString::IsBreakAt(size_t offset)
{
  return (CheckAt('\r', offset)                                       // CR (#xD)
          || CheckAt('\n', offset)                                    // LF (#xA)
          || (CheckAt('\xC2', offset) && CheckAt('\x85', offset + 1)) // NEL (#x85)
          || (CheckAt('\xE2', offset) && CheckAt('\x80', offset + 1) &&
              CheckAt('\xA8', offset + 2)) // LS (#x2028)
          || (CheckAt('\xE2', offset) && CheckAt('\x80', offset + 1) &&
              CheckAt('\xA9', offset + 2))) /* PS (#x2029) */;
}

bool YamlString::IsCrlfAt(size_t offset)
{
  return CheckAt('\r', offset) && CheckAt('\n', offset + 1);
}

/*
 * Check if the character is a line break or NUL.
 */
bool YamlString::IsBreakOrNulAt(size_t offset)
{
  return (IsBreakAt(offset) || IsNulAt(offset));
}

/*
 * Check if the character is a line break, space, or NUL.
 */
bool YamlString::IsSpaceOrNulAt(size_t offset)
{
  return (IsSpaceAt(offset) || IsBreakOrNulAt(offset));
}

/*
 * Check if the character is a line break, space, tab, or NUL.
 */
bool YamlString::IsBlankOrNulAt(size_t offset)
{
  return (IsBlankAt(offset) || IsBreakOrNulAt(offset));
}

/*
 * Determine the width of the character.
 */
size_t YamlString::WidthAt(size_t offset)
{
  return ((this->pointer[offset] & 0x80) == 0x00
              ? 1
              : (this->pointer[offset] & 0xE0) == 0xC0
                    ? 2
                    : (this->pointer[offset] & 0xF0) == 0xE0
                          ? 3
                          : (this->pointer[offset] & 0xF8) == 0xF0 ? 4 : 0);
}

/*
 * Move the string pointer to the next character.
 */
bool YamlString::MOVE()
{
  return (this->pointer += WidthAt());
}

/*
 * Copy a character and move the pointers of both strings.
 */
bool YamlString::COPY(YamlString& string_b)
{
  return ((*(string_b).pointer & 0x80) == 0x00
              ? (*(this->pointer++) = *((string_b).pointer++))
              : (*(string_b).pointer & 0xE0) == 0xC0
                    ? (*(this->pointer++) = *((string_b).pointer++),
                       *(this->pointer++) = *((string_b).pointer++))
                    : (*(string_b).pointer & 0xF0) == 0xE0
                          ? (*(this->pointer++) = *((string_b).pointer++),
                             *(this->pointer++) = *((string_b).pointer++),
                             *(this->pointer++) = *((string_b).pointer++))
                          : (*(string_b).pointer & 0xF8) == 0xF0
                                ? (*(this->pointer++) = *((string_b).pointer++),
                                   *(this->pointer++) = *((string_b).pointer++),
                                   *(this->pointer++) = *((string_b).pointer++),
                                   *(this->pointer++) = *((string_b).pointer++))
                                : 0);
}

// YamlStack

template <typename T>
bool YamlStack<T>::Init(YamlParser& parser)
{
  this->start = (T*)parser.Malloc(INITIAL_STACK_SIZE * sizeof(*this->start));
  if (this->start)
  {
    this->top = this->start;
    this->end = this->start + INITIAL_STACK_SIZE;
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

template <typename T>
void YamlStack<T>::Del(YamlParser& parser)
{
  parser.Free(this->start);
  this->start = nullptr;
  this->top   = nullptr;
  this->end   = nullptr;
}

template <typename T>
bool YamlStack<T>::Empty()
{
  return (this->start == this->top);
}

template <typename T>
bool YamlStack<T>::Limit(YamlParser& parser, size_t size)
{
  if ((this->top - this->start) < size)
  {
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

template <typename T>
bool YamlStack<T>::Push(YamlParser& parser, const T& value)
{
  if (this->top != this->end || this->Extend(parser))
  {
    *(this->top++) = value;
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

template <typename T>
const T& YamlStack<T>::Pop()
{
  return (*(--this->top));
}

/*
 * Extend a stack.
 */
template <typename T>
bool YamlStack<T>::Extend(YamlParser& parser)
{
  T* new_start;

  if ((char*)this->end - (char*)this->start >= INT_MAX / 2)
  {
    return false;
  }

  new_start = (T*)parser.Realloc(this->start, ((char*)this->end - (char*)this->start) * 2);

  if (!new_start)
  {
    return false;
  }

  this->top   = new_start + (this->top - this->start);
  this->end   = new_start + (this->end - this->start) * 2;
  this->start = new_start;

  return true;
}

// YamlQueue

template <typename T>
bool YamlQueue<T>::Init(YamlParser& parser, size_t size)
{
  this->start = (T*)parser.Malloc(size * sizeof(T));
  if (this->start)
  {
    this->head = this->start;
    this->tail = this->start;
    this->end  = this->start + size;
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

template <typename T>
void YamlQueue<T>::Del(YamlParser& parser)
{
  parser.Free(this->start);
  this->start = nullptr;
  this->head  = nullptr;
  this->tail  = nullptr;
  this->end   = nullptr;
}

template <typename T>
bool YamlQueue<T>::Empty()
{
  return (this->head == this->tail);
}

template <typename T>
bool YamlQueue<T>::Enqueue(YamlParser& parser, T value)
{
  if (this->tail != this->end || this->Extend(parser))
  {
    *(this->tail++) = value;
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

template <typename T>
T YamlQueue<T>::Dequeue()
{
  return *(this->head++);
}

template <typename T>
bool YamlQueue<T>::YamlQueue::Insert(YamlParser& parser, size_t index, T value)
{
  if (this->tail != this->end || this->Extend(parser))
  {
    memmove(this->head + index + 1, this->head + index,
            (this->tail - this->head - index) * sizeof(T));
    *(this->head + index) = value;
    this->tail++;
    return true;
  }
  else
  {
    parser.error = EYamlError::Memory;
    return false;
  }
}

/*
 * Extend or move a queue.
 */
template <typename T>
bool YamlQueue<T>::Extend(YamlParser& parser)
{
  // Check if we need to resize the queue.
  if (this->start == this->head && this->tail == this->end)
  {
    T* new_start = (T*)parser.Realloc(this->start, ((char*)this->end - (char*)this->start) * 2);

    if (!new_start)
    {
      return false;
    }

    this->head  = new_start + (this->head - this->start);
    this->tail  = new_start + (this->tail - this->start);
    this->end   = new_start + (this->end - this->start) * 2;
    this->start = new_start;
  }

  // Check if we need to move the queue at the beginning of the buffer.
  if (this->tail == this->end)
  {
    if (this->head != this->tail)
    {
      memmove(this->start, this->head, (char*)this->tail - (char*)this->head);
    }
    this->tail = this->tail - this->head + this->start;
    this->head = this->start;
  }

  return true;
}

// YamlToken

YamlToken YamlToken::Init(EYamlTokenType type, const YamlMark& start_mark, const YamlMark& end_mark)
{
  YamlToken token  = {};
  token.type       = type;
  token.start_mark = start_mark;
  token.end_mark   = end_mark;
  return token;
}

YamlToken YamlToken::InitStreamStart(EYamlEncoding encoding, const YamlMark& start_mark,
                                     const YamlMark& end_mark)
{
  YamlToken token = YamlToken::Init(EYamlTokenType::StreamStart, start_mark, end_mark);
  stream_start_t stream_start;
  stream_start.encoding = encoding;
  token.data            = stream_start;
  return token;
}

YamlToken YamlToken::InitStreamEnd(const YamlMark& start_mark, const YamlMark& end_mark)
{
  return YamlToken::Init(EYamlTokenType::StreamEnd, start_mark, end_mark);
}

YamlToken YamlToken::InitAlias(uint8_t* value, const YamlMark& start_mark, const YamlMark& end_mark)
{
  YamlToken token = YamlToken::Init(EYamlTokenType::Alias, start_mark, end_mark);
  alias_t alias;
  alias.value = value;
  token.data  = alias;
  return token;
}

YamlToken YamlToken::InitAnchor(uint8_t* value, const YamlMark& start_mark,
                                const YamlMark& end_mark)
{
  YamlToken token = YamlToken::Init(EYamlTokenType::Anchor, start_mark, end_mark);
  anchor_t anchor;
  anchor.value = value;
  token.data   = anchor;
  return token;
}

YamlToken YamlToken::InitTag(uint8_t* handle, uint8_t* suffix, const YamlMark& start_mark,
                             const YamlMark& end_mark)
{
  YamlToken token = YamlToken::Init(EYamlTokenType::Tag, start_mark, end_mark);
  tag_t tag;
  tag.handle = handle;
  tag.suffix = suffix;
  token.data = tag;
  return token;
}

YamlToken YamlToken::InitScalar(uint8_t* value, size_t length, EYamlScalarStyle style,
                                const YamlMark& start_mark, const YamlMark& end_mark)
{
  YamlToken token = YamlToken::Init(EYamlTokenType::Scalar, start_mark, end_mark);
  scalar_t scalar;
  scalar.value  = value;
  scalar.length = length;
  scalar.style  = style;
  token.data    = scalar;
  return token;
}

YamlToken YamlToken::InitVersionDirective(int major, int minor, const YamlMark& start_mark,
                                          const YamlMark& end_mark)
{
  YamlToken token = YamlToken::Init(EYamlTokenType::VersionDirective, start_mark, end_mark);
  version_directive_t version_directive;
  version_directive.major = major;
  version_directive.minor = minor;
  token.data              = version_directive;
  return token;
}

YamlToken YamlToken::InitTagDirective(uint8_t* handle, uint8_t* prefix, const YamlMark& start_mark,
                                      const YamlMark& end_mark)
{
  YamlToken token = YamlToken::Init(EYamlTokenType::TagDirective, start_mark, end_mark);
  tag_directive_t tag_directive;
  tag_directive.handle = handle;
  tag_directive.prefix = prefix;
  token.data           = tag_directive;
  return token;
}

bool YamlParser::SetReaderError(const char* problem, size_t offset, int value)
{
  this->error          = EYamlError::Reader;
  this->problem        = problem;
  this->problem_offset = offset;
  this->problem_value  = value;

  return false;
}

constexpr auto BOM_UTF8      = "\xef\xbb\xbf";
constexpr auto BOM_UTF16LE   = "\xff\xfe";
constexpr auto BOM_UTF16BE   = "\xfe\xff";
constexpr auto MAX_FILE_SIZE = (~(size_t)0 / 2);

bool YamlParser::DetermineEncoding()
{
  // Ensure that we had enough bytes in the raw buffer.
  while (!this->eof && ((this->raw_buffer.last - this->raw_buffer.pointer) < 3))
  {
    if (!this->UpdateRawBuffer())
    {
      return false;
    }
  }

  // Determine the encoding.
  if (this->raw_buffer.last - this->raw_buffer.pointer >= 2 &&
      !memcmp(this->raw_buffer.pointer, BOM_UTF16LE, 2))
  {
    this->encoding = EYamlEncoding::Utf16Le;
    this->raw_buffer.pointer += 2;
    this->offset += 2;
  }
  else if (this->raw_buffer.last - this->raw_buffer.pointer >= 2 &&
           !memcmp(this->raw_buffer.pointer, BOM_UTF16BE, 2))
  {
    this->encoding = EYamlEncoding::Utf16Be;
    this->raw_buffer.pointer += 2;
    this->offset += 2;
  }
  else if (this->raw_buffer.last - this->raw_buffer.pointer >= 3 &&
           !memcmp(this->raw_buffer.pointer, BOM_UTF8, 3))
  {
    this->encoding = EYamlEncoding::Utf8;
    this->raw_buffer.pointer += 3;
    this->offset += 3;
  }
  else
  {
    this->encoding = EYamlEncoding::Utf8;
  }

  return true;
}

/*
 * Update the raw buffer.
 */
bool YamlParser::UpdateRawBuffer()
{
  size_t size_read = 0;

  // Return if the raw buffer is full.
  if (this->raw_buffer.start == this->raw_buffer.pointer &&
      this->raw_buffer.last == this->raw_buffer.end)
    return true;

  // Return on EOF.
  if (this->eof) return true;

  // Move the remaining bytes in the raw buffer to the beginning.
  if (this->raw_buffer.start < this->raw_buffer.pointer &&
      this->raw_buffer.pointer < this->raw_buffer.last)
  {
    memmove(this->raw_buffer.start, this->raw_buffer.pointer,
            this->raw_buffer.last - this->raw_buffer.pointer);
  }
  this->raw_buffer.last -= this->raw_buffer.pointer - this->raw_buffer.start;
  this->raw_buffer.pointer = this->raw_buffer.start;

  // Call the read handler to fill the buffer.
  if (!this->read_handler(*this, this->raw_buffer.last,
                          this->raw_buffer.end - this->raw_buffer.last, &size_read))
  {
    return this->SetReaderError("input error", this->offset, -1);
  }
  this->raw_buffer.last += size_read;
  if (!size_read)
  {
    this->eof = true;
  }

  return true;
}

bool YamlParser::UpdateBuffer(size_t length)
{
  bool first = true;

  // Read handler must be set.
  assert(this->read_handler);

  // If the EOF flag is set and the raw buffer is empty, do nothing.
  if (this->eof && this->raw_buffer.pointer == this->raw_buffer.last) return true;

  // Return if the buffer contains enough characters.
  if (this->unread >= length) return true;

  // Determine the input encoding if it is not known yet.
  if (this->encoding == EYamlEncoding::Any)
  {
    if (!this->DetermineEncoding())
    {
      return false;
    }
  }

  if (this->buffer.start < this->buffer.pointer && this->buffer.pointer < this->buffer.last)
  {
    size_t size = this->buffer.last - this->buffer.pointer;
    memmove(this->buffer.start, this->buffer.pointer, size);
    this->buffer.pointer = this->buffer.start;
    this->buffer.last    = this->buffer.start + size;
  }
  else if (this->buffer.pointer == this->buffer.last)
  {
    this->buffer.pointer = this->buffer.start;
    this->buffer.last    = this->buffer.start;
  }

  // Fill the buffer until it has enough characters.
  while (this->unread < length)
  {
    // Fill the raw buffer if necessary.
    if (!first || this->raw_buffer.pointer == this->raw_buffer.last)
    {
      if (!this->UpdateRawBuffer())
      {
        return false;
      }
    }
    first = false;

    // Decode the raw buffer.
    while (this->raw_buffer.pointer != this->raw_buffer.last)
    {
      unsigned int value = 0, value2 = 0;
      bool incomplete = false;
      unsigned char octet;
      unsigned int width = 0;
      int low, high;
      size_t raw_unread = this->raw_buffer.last - this->raw_buffer.pointer;

      // Decode the next character.
      switch (this->encoding)
      {
      case EYamlEncoding::Utf8:
        /*
         * Decode a UTF-8 character.  Check RFC 3629
         * (http://www.ietf.org/rfc/rfc3629.txt) for more details.
         *
         * The following table (taken from the RFC) is used for
         * decoding.
         *
         *    Char. number range |        UTF-8 octet sequence
         *      (hexadecimal)    |              (binary)
         *   --------------------+------------------------------------
         *   0000 0000-0000 007F | 0xxxxxxx
         *   0000 0080-0000 07FF | 110xxxxx 10xxxxxx
         *   0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
         *   0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
         *
         * Additionally, the characters in the range 0xD800-0xDFFF
         * are prohibited as they are reserved for use with UTF-16
         * surrogate pairs.
         */
        octet = this->raw_buffer.pointer[0];
        width = (octet & 0x80) == 0x00
                    ? 1
                    : (octet & 0xE0) == 0xC0
                          ? 2
                          : (octet & 0xF0) == 0xE0 ? 3 : (octet & 0xF8) == 0xF0 ? 4 : 0;

        // Check if the leading octet is valid.
        if (!width)
        {
          return this->SetReaderError("invalid leading UTF-8 octet", this->offset, octet);
        }

        // Check if the raw buffer contains an incomplete character.
        if (width > raw_unread)
        {
          if (this->eof)
          {
            return this->SetReaderError("incomplete UTF-8 octet sequence", this->offset, -1);
          }
          incomplete = true;
          break;
        }

        // Decode the leading octet.
        value = (octet & 0x80) == 0x00
                    ? octet & 0x7F
                    : (octet & 0xE0) == 0xC0
                          ? octet & 0x1F
                          : (octet & 0xF0) == 0xE0 ? octet & 0x0F
                                                   : (octet & 0xF8) == 0xF0 ? octet & 0x07 : 0;

        // Check and decode the trailing octets.
        for (size_t k = 1; k < width; k++)
        {
          octet = this->raw_buffer.pointer[k];

          // Check if the octet is valid.
          if ((octet & 0xC0) != 0x80)
          {
            return this->SetReaderError("invalid trailing UTF-8 octet", this->offset + k, octet);
          }

          // Decode the octet.
          value = (value << 6) + (octet & 0x3F);
        }

        // Check the length of the sequence against the value.
        if (!((width == 1) || (width == 2 && value >= 0x80) || (width == 3 && value >= 0x800) ||
              (width == 4 && value >= 0x10000)))
          return this->SetReaderError("invalid length of a UTF-8 sequence", this->offset, -1);

        // Check the range of the value.
        if ((value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF)
          return this->SetReaderError("invalid Unicode character", this->offset, value);

        break;

      case EYamlEncoding::Utf16Le:
      case EYamlEncoding::Utf16Be:

        low  = (this->encoding == EYamlEncoding::Utf16Le ? 0 : 1);
        high = (this->encoding == EYamlEncoding::Utf16Le ? 1 : 0);

        /*
         * The UTF-16 encoding is not as simple as one might
         * naively think.  Check RFC 2781
         * (http://www.ietf.org/rfc/rfc2781.txt).
         *
         * Normally, two subsequent bytes describe a Unicode
         * character.  However a special technique (called a
         * surrogate pair) is used for specifying character
         * values larger than 0xFFFF.
         *
         * A surrogate pair consists of two pseudo-characters:
         *      high surrogate area (0xD800-0xDBFF)
         *      low surrogate area (0xDC00-0xDFFF)
         *
         * The following formulas are used for decoding
         * and encoding characters using surrogate pairs:
         *
         *  U  = U' + 0x10000   (0x01 00 00 <= U <= 0x10 FF FF)
         *  U' = yyyyyyyyyyxxxxxxxxxx   (0 <= U' <= 0x0F FF FF)
         *  W1 = 110110yyyyyyyyyy
         *  W2 = 110111xxxxxxxxxx
         *
         * where U is the character value, W1 is the high surrogate
         * area, W2 is the low surrogate area.
         */

        // Check for incomplete UTF-16 character.
        if (raw_unread < 2)
        {
          if (this->eof)
          {
            return this->SetReaderError("incomplete UTF-16 character", this->offset, -1);
          }
          incomplete = true;
          break;
        }

        // Get the character.
        value = this->raw_buffer.pointer[low] + (this->raw_buffer.pointer[high] << 8);

        // Check for unexpected low surrogate area.
        if ((value & 0xFC00) == 0xDC00)
          return this->SetReaderError("unexpected low surrogate area", this->offset, value);

        // Check for a high surrogate area.
        if ((value & 0xFC00) == 0xD800)
        {
          width = 4;

          // Check for incomplete surrogate pair.
          if (raw_unread < 4)
          {
            if (this->eof)
            {
              return this->SetReaderError("incomplete UTF-16 surrogate pair", this->offset, -1);
            }
            incomplete = true;
            break;
          }

          // Get the next character.
          value2 = this->raw_buffer.pointer[low + 2] + (this->raw_buffer.pointer[high + 2] << 8);

          // Check for a low surrogate area.
          if ((value2 & 0xFC00) != 0xDC00)
            return this->SetReaderError("expected low surrogate area", this->offset + 2, value2);

          // Generate the value of the surrogate pair.
          value = 0x10000 + ((value & 0x3FF) << 10) + (value2 & 0x3FF);
        }

        else
        {
          width = 2;
        }

        break;

      default:
        assert(1); // Impossible.
      }

      // Check if the raw buffer contains enough bytes to form a character.
      if (incomplete) break;

      /*
       * Check if the character is in the allowed range:
       *      #x9 | #xA | #xD | [#x20-#x7E]               (8 bit)
       *      | #x85 | [#xA0-#xD7FF] | [#xE000-#xFFFD]    (16 bit)
       *      | [#x10000-#x10FFFF]                        (32 bit)
       */
      if (!(value == 0x09 || value == 0x0A || value == 0x0D || (value >= 0x20 && value <= 0x7E) ||
            (value == 0x85) || (value >= 0xA0 && value <= 0xD7FF) ||
            (value >= 0xE000 && value <= 0xFFFD) || (value >= 0x10000 && value <= 0x10FFFF)))
        return this->SetReaderError("control characters are not allowed", this->offset, value);

      // Move the raw pointers.
      this->raw_buffer.pointer += width;
      this->offset += width;

      // Finally put the character into the buffer.

      // 0000 0000-0000 007F -> 0xxxxxxx
      if (value <= 0x7F)
      {
        *(this->buffer.last++) = value;
      }
      // 0000 0080-0000 07FF -> 110xxxxx 10xxxxxx
      else if (value <= 0x7FF)
      {
        *(this->buffer.last++) = 0xC0 + (value >> 6);
        *(this->buffer.last++) = 0x80 + (value & 0x3F);
      }
      // 0000 0800-0000 FFFF -> 1110xxxx 10xxxxxx 10xxxxxx
      else if (value <= 0xFFFF)
      {
        *(this->buffer.last++) = 0xE0 + (value >> 12);
        *(this->buffer.last++) = 0x80 + ((value >> 6) & 0x3F);
        *(this->buffer.last++) = 0x80 + (value & 0x3F);
      }
      // 0001 0000-0010 FFFF -> 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      else
      {
        *(this->buffer.last++) = 0xF0 + (value >> 18);
        *(this->buffer.last++) = 0x80 + ((value >> 12) & 0x3F);
        *(this->buffer.last++) = 0x80 + ((value >> 6) & 0x3F);
        *(this->buffer.last++) = 0x80 + (value & 0x3F);
      }

      this->unread++;
    }

    // On EOF, put NUL into the buffer and return.
    if (this->eof)
    {
      *(this->buffer.last++) = '\0';
      this->unread++;
      return true;
    }
  }

  if (this->offset >= MAX_FILE_SIZE)
  {
    return this->SetReaderError("input is too long", this->offset, -1);
  }

  return true;
}

// YamlParser

bool YamlParser::Cache(size_t length)
{
  if (this->unread >= length)
  {
    return true;
  }
  else
  {
    return this->UpdateBuffer(length);
  }
}

void YamlParser::Skip()
{
  this->mark.index++;
  this->mark.column++;
  this->unread--;
  this->buffer.pointer += this->buffer.WidthAt();
}

void YamlParser::SkipLine()
{
  if (this->buffer.IsCrlfAt())
  {
    this->mark.index += 2, this->mark.column = 0, this->mark.line++, this->unread -= 2;
    this->buffer.pointer += 2;
  }
  else if (this->buffer.IsBreakAt())
  {
    this->mark.index++;
    this->mark.column = 0;
    this->mark.line++;
    this->unread--;
    this->buffer.pointer += this->buffer.WidthAt();
  }
}

/*
 * Copy a character to a string buffer and advance pointers.
 */
bool YamlParser::Read(YamlString& string)
{
  return (string.Extend(*this) ? (string.COPY(this->buffer), this->mark.index++,
                                  this->mark.column++, this->unread--, 1)
                               : 0);
}

/*
 * Copy a line break character to a string buffer and advance pointers.
 */
bool YamlParser::ReadLine(YamlString& string)
{
  return (string.Extend(*this)
              ? (((this->buffer.CheckAt('\r', 0) && this->buffer.CheckAt('\n', 1))
                      ? // CR LF -> LF
                      (*((string).pointer++) = (uint8_t)'\n', this->buffer.pointer += 2,
                       this->mark.index += 2, this->mark.column = 0, this->mark.line++,
                       this->unread -= 2)
                      : (this->buffer.CheckAt('\r', 0) || this->buffer.CheckAt('\n', 0))
                            ? // CR|LF -> LF
                            (*((string).pointer++) = (uint8_t)'\n', this->buffer.pointer++,
                             this->mark.index++, this->mark.column = 0, this->mark.line++,
                             this->unread--)
                            : (this->buffer.CheckAt('\xC2', 0) && this->buffer.CheckAt('\x85', 1))
                                  ? // NEL -> LF
                                  (*((string).pointer++) = (uint8_t)'\n', this->buffer.pointer += 2,
                                   this->mark.index++, this->mark.column = 0, this->mark.line++,
                                   this->unread--)
                                  : (this->buffer.CheckAt('\xE2', 0) &&
                                     this->buffer.CheckAt('\x80', 1) &&
                                     (this->buffer.CheckAt('\xA8', 2) ||
                                      this->buffer.CheckAt('\xA9', 2)))
                                        ? // LS|PS -> LS|PS
                                        (*((string).pointer++) = *(this->buffer.pointer++),
                                         *((string).pointer++) = *(this->buffer.pointer++),
                                         *((string).pointer++) = *(this->buffer.pointer++),
                                         this->mark.index++, this->mark.column = 0,
                                         this->mark.line++, this->unread--)
                                        : 0),
                 1)
              : 0);
}

/*
 * Get the next token.
 */
bool YamlParser::Scan(YamlToken& token)
{
  // Erase the token object.
  memset(&token, 0, sizeof(token));

  // No tokens after STREAM-END or error.
  if (this->stream_end_produced || (this->error != EYamlError::None))
  {
    return true;
  }

  // Ensure that the tokens queue contains enough tokens.
  if (!this->token_available)
  {
    if (!this->FetchMoreTokens())
    {
      return false;
    }
  }

  // Fetch the next token from the queue.
  token                 = this->tokens.Dequeue();
  this->token_available = false;
  this->tokens_parsed++;

  if (token.type == EYamlTokenType::StreamEnd)
  {
    this->stream_end_produced = true;
  }

  return true;
}

/*
 * Set the scanner error and return false.
 */
bool YamlParser::SetScannerError(const char* context, YamlMark context_mark, const char* problem)
{
  this->error        = EYamlError::Scanner;
  this->context      = context;
  this->context_mark = context_mark;
  this->problem      = problem;
  this->problem_mark = this->mark;

  return false;
}

/*
 * Ensure that the tokens queue contains at least one token which can be
 * returned to the Parser.
 */
bool YamlParser::FetchMoreTokens()
{
  bool need_more_tokens;

  // While we need more tokens to fetch, do it.
  while (1)
  {
    // Check if we really need to fetch more tokens.
    need_more_tokens = false;

    if (this->tokens.head == this->tokens.tail)
    {
      // Queue is empty.
      need_more_tokens = true;
    }
    else
    {
      YamlSimpleKey* simple_key;

      // Check if any potential simple key may occupy the head position.
      if (!this->StaleSimpleKeys())
      {
        return false;
      }

      for (simple_key = this->simple_keys.start; simple_key != this->simple_keys.top; simple_key++)
      {
        if (simple_key->possible && simple_key->token_number == this->tokens_parsed)
        {
          need_more_tokens = true;
          break;
        }
      }
    }

    // We are finished.
    if (!need_more_tokens) break;

    // Fetch the next token.
    if (!this->FetchNextToken())
    {
      return false;
    }
  }

  this->token_available = true;

  return true;
}

/*
 * The dispatcher for token fetchers.
 */
bool YamlParser::FetchNextToken()
{
  // Ensure that the buffer is initialized.
  if (!YamlParser::Cache(1))
  {
    return false;
  }

  // Check if we just started scanning.  Fetch STREAM-START then.
  if (!this->stream_start_produced) return this->FetchStreamStart();

  // Eat whitespaces and comments until we reach the next token.
  if (!this->ScanToNextToken())
  {
    return false;
  }

  // Remove obsolete potential simple keys.
  if (!this->StaleSimpleKeys())
  {
    return false;
  }

  // Check the indentation level against the current column.
  if (!this->UnrollIndent(this->mark.column))
  {
    return false;
  }

  /*
   * Ensure that the buffer contains at least 4 characters.  4 is the length
   * of the longest indicators ('--- ' and '... ').
   */
  if (!this->Cache(4))
  {
    return false;
  }

  // Is it the end of the stream?
  if (this->buffer.IsNulAt()) return this->FetchStreamEnd();

  // Is it a directive?
  if (this->mark.column == 0 && this->buffer.CheckAt('%')) return this->FetchDirective();

  // Is it the document start indicator?
  if (this->mark.column == 0 && this->buffer.CheckAt('-', 0) && this->buffer.CheckAt('-', 1) &&
      this->buffer.CheckAt('-', 2) && this->buffer.IsBlankOrNulAt(3))
    return this->FetchDocumentIndicator(EYamlTokenType::DocumentStart);

  // Is it the document end indicator?
  if (this->mark.column == 0 && this->buffer.CheckAt('.', 0) && this->buffer.CheckAt('.', 1) &&
      this->buffer.CheckAt('.', 2) && this->buffer.IsBlankOrNulAt(3))
    return this->FetchDocumentIndicator(EYamlTokenType::DocumentEnd);

  // Is it the flow sequence start indicator?
  if (this->buffer.CheckAt('['))
    return this->FetchFlowCollectionStart(EYamlTokenType::FlowSequenceStart);

  // Is it the flow mapping start indicator?
  if (this->buffer.CheckAt('{'))
    return this->FetchFlowCollectionStart(EYamlTokenType::FlowMappingStart);

  // Is it the flow sequence end indicator?
  if (this->buffer.CheckAt(']'))
    return this->FetchFlowCollectionEnd(EYamlTokenType::FlowSequenceEnd);

  // Is it the flow mapping end indicator?
  if (this->buffer.CheckAt('}'))
    return this->FetchFlowCollectionEnd(EYamlTokenType::FlowMappingEnd);

  // Is it the flow entry indicator?
  if (this->buffer.CheckAt(',')) return this->FetchFlowEntry();

  // Is it the block entry indicator?
  if (this->buffer.CheckAt('-') && this->buffer.IsBlankOrNulAt(1)) return this->FetchBlockEntry();

  // Is it the key indicator?
  if (this->buffer.CheckAt('?') && (this->flow_level || this->buffer.IsBlankOrNulAt(1)))
    return this->FetchKey();

  // Is it the value indicator?
  if (this->buffer.CheckAt(':') && (this->flow_level || this->buffer.IsBlankOrNulAt(1)))
    return this->FetchValue();

  // Is it an alias?
  if (this->buffer.CheckAt('*')) return this->FetchAnchor(EYamlTokenType::Alias);

  // Is it an anchor?
  if (this->buffer.CheckAt('&')) return this->FetchAnchor(EYamlTokenType::Anchor);

  // Is it a tag?
  if (this->buffer.CheckAt('!')) return this->FetchTag();

  // Is it a literal scalar?
  if (this->buffer.CheckAt('|') && !this->flow_level) return this->FetchBlockScalar(true);

  // Is it a folded scalar?
  if (this->buffer.CheckAt('>') && !this->flow_level) return this->FetchBlockScalar(false);

  // Is it a single-quoted scalar?
  if (this->buffer.CheckAt('\'')) return this->FetchFlowScalar(true);

  // Is it a double-quoted scalar?
  if (this->buffer.CheckAt('"')) return this->FetchFlowScalar(false);

  /*
   * Is it a plain scalar?
   *
   * A plain scalar may start with any non-blank characters except
   *
   *      '-', '?', ':', ',', '[', ']', '{', '}',
   *      '#', '&', '*', '!', '|', '>', '\'', '\"',
   *      '%', '@', '`'.
   *
   * In the block context (and, for the '-' indicator, in the flow context
   * too), it may also start with the characters
   *
   *      '-', '?', ':'
   *
   * if it is followed by a non-space character.
   *
   * The last rule is more restrictive than the specification requires.
   */
  if (!(this->buffer.IsBlankOrNulAt() || this->buffer.CheckAt('-') || this->buffer.CheckAt('?') ||
        this->buffer.CheckAt(':') || this->buffer.CheckAt(',') || this->buffer.CheckAt('[') ||
        this->buffer.CheckAt(']') || this->buffer.CheckAt('{') || this->buffer.CheckAt('}') ||
        this->buffer.CheckAt('#') || this->buffer.CheckAt('&') || this->buffer.CheckAt('*') ||
        this->buffer.CheckAt('!') || this->buffer.CheckAt('|') || this->buffer.CheckAt('>') ||
        this->buffer.CheckAt('\'') || this->buffer.CheckAt('"') || this->buffer.CheckAt('%') ||
        this->buffer.CheckAt('@') || this->buffer.CheckAt('`')) ||
      (this->buffer.CheckAt('-') && !this->buffer.IsBlankAt(1)) ||
      (!this->flow_level && (this->buffer.CheckAt('?') || this->buffer.CheckAt(':')) &&
       !this->buffer.IsBlankOrNulAt(1)))
    return this->FetchPlainScalar();

  /*
   * If we don't determine the token type so far, it is an error.
   */
  return this->SetScannerError("while scanning for the next token", this->mark,
                               "found character that cannot start any token");
}

/*
 * Check the list of potential simple keys and remove the positions that
 * cannot contain simple keys anymore.
 */
bool YamlParser::StaleSimpleKeys()
{
  YamlSimpleKey* simple_key;

  // Check for a potential simple key for each flow level.
  for (simple_key = this->simple_keys.start; simple_key != this->simple_keys.top; simple_key++)
  {
    /*
     * The specification requires that a simple key
     *
     *  - is limited to a single line,
     *  - is shorter than 1024 characters.
     */
    if (simple_key->possible && (simple_key->mark.line < this->mark.line ||
                                 simple_key->mark.index + 1024 < this->mark.index))
    {
      // Check if the potential simple key to be removed is required.
      if (simple_key->required)
      {
        return this->SetScannerError("while scanning a simple key", simple_key->mark,
                                     "could not find expected ':'");
      }

      simple_key->possible = false;
    }
  }

  return true;
}

/*
 * Check if a simple key may start at the current position and add it if
 * needed.
 */
bool YamlParser::SaveSimpleKey()
{
  /*
   * A simple key is required at the current position if the scanner is in
   * the block context and the current column coincides with the indentation
   * level.
   */
  int required = (!this->flow_level && this->indent == (ptrdiff_t)this->mark.column);

  /*
   * If the current position may start a simple key, save it.
   */
  if (this->simple_key_allowed)
  {
    YamlSimpleKey simple_key;
    simple_key.possible     = true;
    simple_key.required     = required;
    simple_key.token_number = this->tokens_parsed + (this->tokens.tail - this->tokens.head);
    simple_key.mark         = this->mark;

    if (!this->RemoveSimpleKey())
    {
      return false;
    }

    *(this->simple_keys.top - 1) = simple_key;
  }

  return true;
}

/*
 * Remove a potential simple key at the current flow level.
 */
bool YamlParser::RemoveSimpleKey()
{
  YamlSimpleKey* simple_key = this->simple_keys.top - 1;

  if (simple_key->possible)
  {
    // If the key is required, it is an error.
    if (simple_key->required)
    {
      return this->SetScannerError("while scanning a simple key", simple_key->mark,
                                   "could not find expected ':'");
    }
  }

  // Remove the key from the stack.
  simple_key->possible = false;

  return true;
}

/*
 * Increase the flow level and resize the simple key list if needed.
 */
bool YamlParser::IncreaseFlowLevel()
{
  YamlSimpleKey empty_simple_key = {0, 0, 0, {0, 0, 0}};

  // Reset the simple key on the next level.
  if (!this->simple_keys.Push(*this, empty_simple_key))
  {
    return false;
  }

  // Increase the flow level.
  if (this->flow_level == INT_MAX)
  {
    this->error = EYamlError::Memory;
    return false;
  }

  this->flow_level++;

  return true;
}

/*
 * Decrease the flow level.
 */
bool YamlParser::DecreaseFlowLevel()
{
  if (this->flow_level)
  {
    this->flow_level--;
    (void)this->simple_keys.Pop();
  }

  return true;
}

/*
 * Push the current indentation level to the stack and set the new level
 * the current column is greater than the indentation level.  In this case,
 * append or insert the specified token into the token queue.
 *
 */
bool YamlParser::RollIndent(ptrdiff_t column, ptrdiff_t number, EYamlTokenType type, YamlMark mark)
{
  // In the flow context, do nothing.
  if (this->flow_level)
  {
    return true;
  }

  if (this->indent < column)
  {
    /*
     * Push the current indentation level to the stack and set the new
     * indentation level.
     */
    if (!this->indents.Push(*this, this->indent))
    {
      return false;
    }

    if (column > INT_MAX)
    {
      this->error = EYamlError::Memory;
      return false;
    }

    this->indent = (int)column;

    // Create a token and insert it into the queue.
    YamlToken token = YamlToken::Init(type, mark, mark);

    if (number == -1)
    {
      if (!this->tokens.Enqueue(*this, token))
      {
        return false;
      }
    }
    else if (!this->tokens.Insert(*this, number - this->tokens_parsed, token))
    {
      return false;
    }
  }

  return true;
}

/*
 * Pop indentation levels from the indents stack until the current level
 * becomes less or equal to the column.  For each indentation level, append
 * the BLOCK-END token.
 */
bool YamlParser::UnrollIndent(ptrdiff_t column)
{
  YamlToken token;

  // In the flow context, do nothing.
  if (this->flow_level)
  {
    return true;
  }

  // Loop through the indentation levels in the stack.
  while (this->indent > column)
  {
    // Create a token and append it to the queue.
    YamlToken token = YamlToken::Init(EYamlTokenType::BlockEnd, this->mark, this->mark);

    if (!this->tokens.Enqueue(*this, token))
    {
      return false;
    }

    // Pop the indentation level.
    this->indent = this->indents.Pop();
  };

  return true;
}

/*
 * Initialize the scanner and produce the STREAM-START token.
 */
bool YamlParser::FetchStreamStart()
{
  YamlSimpleKey simple_key = {0, 0, 0, {0, 0, 0}};

  // Set the initial indentation.
  this->indent = -1;

  // Initialize the simple key stack.
  if (!this->simple_keys.Push(*this, simple_key))
  {
    return false;
  }

  // A simple key is allowed at the beginning of the stream.
  this->simple_key_allowed = true;

  // We have started.
  this->stream_start_produced = true;

  // Create the STREAM-START token and append it to the queue.
  YamlToken token = YamlToken::InitStreamStart(this->encoding, this->mark, this->mark);

  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the STREAM-END token and shut down the scanner.
 */
bool YamlParser::FetchStreamEnd()
{
  // Force new line.
  if (this->mark.column != 0)
  {
    this->mark.column = 0;
    this->mark.line++;
  }

  // Reset the indentation level.
  if (!this->UnrollIndent(-1))
  {
    return false;
  }

  // Reset simple keys.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  this->simple_key_allowed = false;

  // Create the STREAM-END token and append it to the queue.
  YamlToken token = YamlToken::InitStreamEnd(this->mark, this->mark);

  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce a VERSION-DIRECTIVE or TAG-DIRECTIVE token.
 */
bool YamlParser::FetchDirective()
{
  YamlToken token;

  // Reset the indentation level.
  if (!this->UnrollIndent(-1))
  {
    return false;
  }

  // Reset simple keys.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  this->simple_key_allowed = false;

  // Create the YAML-DIRECTIVE or TAG-DIRECTIVE token.
  if (!this->ScanDirective(token))
  {
    return false;
  }

  // Append the token to the queue.
  if (!this->tokens.Enqueue(*this, token))
  {
    token.Delete(*this);
    return false;
  }

  return true;
}

/*
 * Produce the DOCUMENT-START or DOCUMENT-END token.
 */
bool YamlParser::FetchDocumentIndicator(EYamlTokenType type)
{
  YamlMark start_mark, end_mark;

  // Reset the indentation level.
  if (!this->UnrollIndent(-1))
  {
    return false;
  }

  // Reset simple keys.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  this->simple_key_allowed = false;

  // Consume the token.
  start_mark = this->mark;

  this->Skip();
  this->Skip();
  this->Skip();

  end_mark = this->mark;

  // Create the DOCUMENT-START or DOCUMENT-END token.
  YamlToken token = YamlToken::Init(type, start_mark, end_mark);

  // Append the token to the queue.
  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the FLOW-SEQUENCE-START or FLOW-MAPPING-START token.
 */
bool YamlParser::FetchFlowCollectionStart(EYamlTokenType type)
{
  YamlMark start_mark, end_mark;

  // The indicators '[' and '{' may start a simple key.
  if (!this->SaveSimpleKey())
  {
    return false;
  }

  // Increase the flow level.
  if (!this->IncreaseFlowLevel())
  {
    return false;
  }

  // A simple key may follow the indicators '[' and '{'.
  this->simple_key_allowed = true;

  // Consume the token.
  start_mark = this->mark;
  this->Skip();
  end_mark = this->mark;

  // Create the FLOW-SEQUENCE-START of FLOW-MAPPING-START token.
  YamlToken token = YamlToken::Init(type, start_mark, end_mark);

  // Append the token to the queue.
  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the FLOW-SEQUENCE-END or FLOW-MAPPING-END token.
 */
bool YamlParser::FetchFlowCollectionEnd(EYamlTokenType type)
{
  YamlMark start_mark, end_mark;

  // Reset any potential simple key on the current flow level.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  // Decrease the flow level.
  if (!this->DecreaseFlowLevel())
  {
    return false;
  }

  // No simple keys after the indicators ']' and '}'.
  this->simple_key_allowed = false;

  // Consume the token.
  start_mark = this->mark;
  this->Skip();
  end_mark = this->mark;

  // Create the FLOW-SEQUENCE-END of FLOW-MAPPING-END token.
  YamlToken token = YamlToken::Init(type, start_mark, end_mark);

  // Append the token to the queue.
  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the FLOW-ENTRY token.
 */
bool YamlParser::FetchFlowEntry()
{
  YamlMark start_mark, end_mark;

  // Reset any potential simple keys on the current flow level.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  // Simple keys are allowed after ','.
  this->simple_key_allowed = true;

  // Consume the token.
  start_mark = this->mark;
  this->Skip();
  end_mark = this->mark;

  // Create the FLOW-ENTRY token and append it to the queue.
  YamlToken token = YamlToken::Init(EYamlTokenType::FlowEntry, start_mark, end_mark);

  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the BLOCK-ENTRY token.
 */
bool YamlParser::FetchBlockEntry()
{
  YamlMark start_mark, end_mark;

  // Check if the scanner is in the block context.
  if (!this->flow_level)
  {
    // Check if we are allowed to start a new entry.
    if (!this->simple_key_allowed)
    {
      return this->SetScannerError(nullptr, this->mark,
                                   "block sequence entries are not allowed in this context");
    }

    // Add the BLOCK-SEQUENCE-START token if needed.
    if (!this->RollIndent(this->mark.column, -1, EYamlTokenType::BlockSequenceStart, this->mark))
      return false;
  }
  else
  {
    /*
     * It is an error for the '-' indicator to occur in the flow context,
     * but we let the Parser detect and report about it because the Parser
     * is able to point to the context.
     */
  }

  // Reset any potential simple keys on the current flow level.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  // Simple keys are allowed after '-'.
  this->simple_key_allowed = true;

  // Consume the token.
  start_mark = this->mark;
  this->Skip();
  end_mark = this->mark;

  // Create the BLOCK-ENTRY token and append it to the queue.
  YamlToken token = YamlToken::Init(EYamlTokenType::BlockEntry, start_mark, end_mark);

  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the KEY token.
 */
bool YamlParser::FetchKey()
{
  YamlMark start_mark, end_mark;

  // In the block context, additional checks are required.
  if (!this->flow_level)
  {
    // Check if we are allowed to start a new key (not necessary simple).
    if (!this->simple_key_allowed)
    {
      return this->SetScannerError(nullptr, this->mark,
                                   "mapping keys are not allowed in this context");
    }

    // Add the BLOCK-MAPPING-START token if needed.
    if (!this->RollIndent(this->mark.column, -1, EYamlTokenType::BlockMappingStart, this->mark))
    {
      return false;
    }
  }

  // Reset any potential simple keys on the current flow level.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  // Simple keys are allowed after '?' in the block context.
  this->simple_key_allowed = (!this->flow_level);

  // Consume the token.
  start_mark = this->mark;
  this->Skip();
  end_mark = this->mark;

  // Create the KEY token and append it to the queue.
  YamlToken token = YamlToken::Init(EYamlTokenType::Key, start_mark, end_mark);

  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the VALUE token.
 */
bool YamlParser::FetchValue()
{
  YamlMark start_mark, end_mark;
  YamlSimpleKey* simple_key = this->simple_keys.top - 1;

  // Have we found a simple key?
  if (simple_key->possible)
  {
    // Create the KEY token and insert it into the queue.
    YamlToken token = YamlToken::Init(EYamlTokenType::Key, simple_key->mark, simple_key->mark);

    if (!this->tokens.Insert(*this, simple_key->token_number - this->tokens_parsed, token))
      return false;

    // In the block context, we may need to add the BLOCK-MAPPING-START token.
    if (!this->RollIndent(simple_key->mark.column, simple_key->token_number,
                          EYamlTokenType::BlockMappingStart, simple_key->mark))
    {
      return false;
    }

    // Remove the simple key.
    simple_key->possible = false;

    // A simple key cannot follow another simple key.
    this->simple_key_allowed = false;
  }
  else
  {
    // The ':' indicator follows a complex key.

    // In the block context, extra checks are required.
    if (!this->flow_level)
    {
      // Check if we are allowed to start a complex value.
      if (!this->simple_key_allowed)
      {
        return this->SetScannerError(nullptr, this->mark,
                                     "mapping values are not allowed in this context");
      }

      // Add the BLOCK-MAPPING-START token if needed.
      if (!this->RollIndent(this->mark.column, -1, EYamlTokenType::BlockMappingStart, this->mark))
      {
        return false;
      }
    }

    // Simple keys after ':' are allowed in the block context.
    this->simple_key_allowed = (!this->flow_level);
  }

  // Consume the token.
  start_mark = this->mark;
  this->Skip();
  end_mark = this->mark;

  // Create the VALUE token and append it to the queue.
  YamlToken token = YamlToken::Init(EYamlTokenType::Value, start_mark, end_mark);

  if (!this->tokens.Enqueue(*this, token))
  {
    return false;
  }

  return true;
}

/*
 * Produce the ALIAS or ANCHOR token.
 */
bool YamlParser::FetchAnchor(EYamlTokenType type)
{
  YamlToken token;

  // An anchor or an alias could be a simple key.
  if (!this->SaveSimpleKey())
  {
    return false;
  }

  // A simple key cannot follow an anchor or an alias.
  this->simple_key_allowed = false;

  // Create the ALIAS or ANCHOR token and append it to the queue.
  if (!this->ScanAnchor(token, type))
  {
    return false;
  }

  if (!this->tokens.Enqueue(*this, token))
  {
    token.Delete(*this);
    return false;
  }
  return true;
}

/*
 * Produce the TAG token.
 */
bool YamlParser::FetchTag()
{
  YamlToken token;

  // A tag could be a simple key.
  if (!this->SaveSimpleKey())
  {
    return false;
  }

  // A simple key cannot follow a tag.
  this->simple_key_allowed = false;

  // Create the TAG token and append it to the queue.
  if (!this->ScanTag(token))
  {
    return false;
  }

  if (!this->tokens.Enqueue(*this, token))
  {
    token.Delete(*this);
    return false;
  }

  return true;
}

/*
 * Produce the SCALAR(...,literal) or SCALAR(...,folded) tokens.
 */
bool YamlParser::FetchBlockScalar(bool literal)
{
  YamlToken token;

  // Remove any potential simple keys.
  if (!this->RemoveSimpleKey())
  {
    return false;
  }

  // A simple key may follow a block scalar.
  this->simple_key_allowed = true;

  // Create the SCALAR token and append it to the queue.
  if (!this->ScanBlockScalar(token, literal))
  {
    return false;
  }

  if (!this->tokens.Enqueue(*this, token))
  {
    token.Delete(*this);
    return false;
  }

  return true;
}

/*
 * Produce the SCALAR(...,single-quoted) or SCALAR(...,double-quoted) tokens.
 */
bool YamlParser::FetchFlowScalar(bool single)
{
  YamlToken token;

  // A plain scalar could be a simple key.
  if (!this->SaveSimpleKey())
  {
    return false;
  }

  // A simple key cannot follow a flow scalar.
  this->simple_key_allowed = false;

  // Create the SCALAR token and append it to the queue.
  if (!this->ScanFlowScalar(token, single))
  {
    return false;
  }

  if (!this->tokens.Enqueue(*this, token))
  {
    token.Delete(*this);
    return false;
  }

  return true;
}

/*
 * Produce the SCALAR(...,plain) token.
 */
bool YamlParser::FetchPlainScalar()
{
  YamlToken token;

  // A plain scalar could be a simple key.
  if (!this->SaveSimpleKey())
  {
    return false;
  }

  // A simple key cannot follow a flow scalar.
  this->simple_key_allowed = false;

  // Create the SCALAR token and append it to the queue.
  if (!this->ScanPlainScalar(token))
  {
    return false;
  }

  if (!this->tokens.Enqueue(*this, token))
  {
    token.Delete(*this);
    return false;
  }

  return true;
}

/*
 * Eat whitespaces and comments until the next token is found.
 */
bool YamlParser::ScanToNextToken()
{
  // Until the next token is not found.
  while (1)
  {
    // Allow the BOM mark to start a line.
    if (!this->Cache(1))
    {
      return false;
    }

    if (this->mark.column == 0 && this->buffer.IsBomAt())
    {
      this->Skip();
    }

    /*
     * Eat whitespaces.
     *
     * Tabs are allowed:
     *
     *  - in the flow context;
     *  - in the block context, but not at the beginning of the line or
     *  after '-', '?', or ':' (complex value).
     */
    if (!this->Cache(1))
    {
      return false;
    }

    while (this->buffer.CheckAt(' ') ||
           ((this->flow_level || !this->simple_key_allowed) && this->buffer.CheckAt('\t')))
    {
      this->Skip();
      if (!this->Cache(1))
      {
        return false;
      }
    }

    // Eat a comment until a line break.
    if (this->buffer.CheckAt('#'))
    {
      while (!this->buffer.IsBreakOrNulAt())
      {
        this->Skip();
        if (!this->Cache(1))
        {
          return false;
        }
      }
    }

    // If it is a line break, eat it.
    if (this->buffer.IsBreakAt())
    {
      if (!this->Cache(2))
      {
        return false;
      }
      this->SkipLine();

      // In the block context, a new line may start a simple key.
      if (!this->flow_level)
      {
        this->simple_key_allowed = true;
      }
    }
    else
    {
      // We have found a token.
      break;
    }
  }

  return true;
}

/*
 * Scan a YAML-DIRECTIVE or TAG-DIRECTIVE token.
 *
 * Scope:
 *      %YAML    1.1    # a comment \n
 *      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *      %TAG    !yaml!  tag:yaml.org,2002:  \n
 *      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 */
bool YamlParser::ScanDirective(YamlToken& token)
{
  YamlMark start_mark, end_mark;
  uint8_t* name = nullptr;
  int major, minor;
  uint8_t *handle = nullptr, *prefix = nullptr;

  // Eat '%'.
  start_mark = this->mark;

  this->Skip();

  // Scan the directive name.
  if (!this->ScanDirectiveName(start_mark, &name)) goto error;

  // Is it a YAML directive?
  if (strcmp((char*)name, "YAML") == 0)
  {
    // Scan the VERSION directive value.
    if (!this->ScanVersionDirectiveValue(start_mark, &major, &minor)) goto error;

    end_mark = this->mark;

    // Create a VERSION-DIRECTIVE token.
    token = YamlToken::InitVersionDirective(major, minor, start_mark, end_mark);
  }

  // Is it a TAG directive?
  else if (strcmp((char*)name, "TAG") == 0)
  {
    // Scan the TAG directive value.
    if (!this->ScanTagDirectiveValue(start_mark, &handle, &prefix)) goto error;

    end_mark = this->mark;

    // Create a TAG-DIRECTIVE token.
    token = YamlToken::InitTagDirective(handle, prefix, start_mark, end_mark);
  }

  // Unknown directive.
  else
  {
    this->SetScannerError("while scanning a directive", start_mark, "found unknown directive name");
    goto error;
  }

  // Eat the rest of the line including any comments.
  if (!this->Cache(1)) goto error;

  while (this->buffer.IsBlankAt())
  {
    this->Skip();
    if (!this->Cache(1)) goto error;
  }

  if (this->buffer.CheckAt('#'))
  {
    while (!this->buffer.IsBreakOrNulAt())
    {
      this->Skip();
      if (!this->Cache(1)) goto error;
    }
  }

  // Check if we are at the end of the line.
  if (!this->buffer.IsBreakOrNulAt())
  {
    this->SetScannerError("while scanning a directive", start_mark,
                          "did not find expected comment or line break");
    goto error;
  }

  // Eat a line break.
  if (this->buffer.IsBreakAt())
  {
    if (!this->Cache(2)) goto error;
    this->SkipLine();
  }

  this->Free(name);

  return true;

error:
  this->Free(prefix);
  this->Free(handle);
  this->Free(name);
  return false;
}

/*
 * Scan the directive name.
 *
 * Scope:
 *      %YAML   1.1     # a comment \n
 *       ^^^^
 *      %TAG    !yaml!  tag:yaml.org,2002:  \n
 *       ^^^
 */
bool YamlParser::ScanDirectiveName(YamlMark start_mark, uint8_t** name)
{
  YamlString string;

  if (!string.Init(*this, INITIAL_STRING_SIZE)) goto error;

  // Consume the directive name.
  if (!this->Cache(1)) goto error;

  while (this->buffer.IsAlphaAt())
  {
    if (!this->Read(string)) goto error;
    if (!this->Cache(1)) goto error;
  }

  // Check if the name is empty.
  if (string.start == string.pointer)
  {
    this->SetScannerError("while scanning a directive", start_mark,
                          "could not find expected directive name");
    goto error;
  }

  // Check for an blank character after the name.
  if (!this->buffer.IsBlankOrNulAt())
  {
    this->SetScannerError("while scanning a directive", start_mark,
                          "found unexpected non-alphabetical character");
    goto error;
  }

  *name = string.start;

  return true;

error:
  string.Del(*this);
  return false;
}

/*
 * Scan the value of VERSION-DIRECTIVE.
 *
 * Scope:
 *      %YAML   1.1     # a comment \n
 *           ^^^^^^
 */
bool YamlParser::ScanVersionDirectiveValue(YamlMark start_mark, int* major, int* minor)
{
  // Eat whitespaces.
  if (!this->Cache(1))
  {
    return false;
  }

  while (this->buffer.IsBlankAt())
  {
    this->Skip();
    if (!this->Cache(1))
    {
      return false;
    }
  }

  // Consume the major version number.
  if (!this->ScanVersionDirectiveNumber(start_mark, major))
  {
    return false;
  }

  // Eat '.'.
  if (!this->buffer.CheckAt('.'))
  {
    return this->SetScannerError("while scanning a %YAML directive", start_mark,
                                 "did not find expected digit or '.' character");
  }

  this->Skip();

  // Consume the minor version number.
  if (!this->ScanVersionDirectiveNumber(start_mark, minor))
  {
    return false;
  }

  return true;
}

#define MAX_NUMBER_LENGTH 9

/*
 * Scan the version number of VERSION-DIRECTIVE.
 *
 * Scope:
 *      %YAML   1.1     # a comment \n
 *              ^
 *      %YAML   1.1     # a comment \n
 *                ^
 */
bool YamlParser::ScanVersionDirectiveNumber(YamlMark start_mark, int* number)
{
  int value     = 0;
  size_t length = 0;

  // Repeat while the next character is digit.
  if (!this->Cache(1))
  {
    return false;
  }

  while (this->buffer.IsDigitAt())
  {
    // Check if the number is too long.
    if (++length > MAX_NUMBER_LENGTH)
    {
      return this->SetScannerError("while scanning a %YAML directive", start_mark,
                                   "found extremely long version number");
    }

    value = value * 10 + this->buffer.AsDigitAt();

    this->Skip();

    if (!this->Cache(1))
    {
      return false;
    }
  }

  // Check if the number was present.
  if (!length)
  {
    return this->SetScannerError("while scanning a %YAML directive", start_mark,
                                 "did not find expected version number");
  }

  *number = value;

  return true;
}

/*
 * Scan the value of a TAG-DIRECTIVE token.
 *
 * Scope:
 *      %TAG    !yaml!  tag:yaml.org,2002:  \n
 *          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 */
bool YamlParser::ScanTagDirectiveValue(YamlMark start_mark, uint8_t** handle, uint8_t** prefix)
{
  uint8_t* handle_value = nullptr;
  uint8_t* prefix_value = nullptr;

  // Eat whitespaces.
  if (!this->Cache(1)) goto error;

  while (this->buffer.IsBlankAt())
  {
    this->Skip();
    if (!this->Cache(1)) goto error;
  }

  // Scan a handle.
  if (!this->ScanTagHandle(true, start_mark, &handle_value)) goto error;

  // Expect a whitespace.
  if (!this->Cache(1)) goto error;

  if (!this->buffer.IsBlankAt())
  {
    this->SetScannerError("while scanning a %TAG directive", start_mark,
                          "did not find expected whitespace");
    goto error;
  }

  // Eat whitespaces.
  while (this->buffer.IsBlankAt())
  {
    this->Skip();
    if (!this->Cache(1)) goto error;
  }

  // Scan a prefix.
  if (!this->ScanTagUri(1, nullptr, start_mark, &prefix_value)) goto error;

  // Expect a whitespace or line break.
  if (!this->Cache(1)) goto error;

  if (!this->buffer.IsBlankOrNulAt())
  {
    this->SetScannerError("while scanning a %TAG directive", start_mark,
                          "did not find expected whitespace or line break");
    goto error;
  }

  *handle = handle_value;
  *prefix = prefix_value;

  return true;

error:
  this->Free(handle_value);
  this->Free(prefix_value);
  return false;
}

bool YamlParser::ScanAnchor(YamlToken& token, EYamlTokenType type)
{
  int length = 0;
  YamlMark start_mark, end_mark;
  YamlString string;

  if (!string.Init(*this, INITIAL_STRING_SIZE)) goto error;

  // Eat the indicator character.
  start_mark = this->mark;

  this->Skip();

  // Consume the value.
  if (!this->Cache(1)) goto error;

  while (this->buffer.IsAlphaAt())
  {
    if (!this->Read(string)) goto error;
    if (!this->Cache(1)) goto error;
    length++;
  }

  end_mark = this->mark;

  /*
   * Check if length of the anchor is greater than 0 and it is followed by
   * a whitespace character or one of the indicators:
   *
   *      '?', ':', ',', ']', '}', '%', '@', '`'.
   */
  if (!length ||
      !(this->buffer.IsBlankOrNulAt() || this->buffer.CheckAt('?') || this->buffer.CheckAt(':') ||
        this->buffer.CheckAt(',') || this->buffer.CheckAt(']') || this->buffer.CheckAt('}') ||
        this->buffer.CheckAt('%') || this->buffer.CheckAt('@') || this->buffer.CheckAt('`')))
  {
    this->SetScannerError(type == EYamlTokenType::Anchor ? "while scanning an anchor"
                                                         : "while scanning an alias",
                          start_mark, "did not find expected alphabetic or numeric character");
    goto error;
  }

  // Create a token.
  if (type == EYamlTokenType::Anchor)
  {
    token = YamlToken::InitAnchor(string.start, start_mark, end_mark);
  }
  else
  {
    token = YamlToken::InitAlias(string.start, start_mark, end_mark);
  }

  return true;

error:
  string.Del(*this);
  return false;
}

/*
 * Scan a TAG token.
 */
bool YamlParser::ScanTag(YamlToken& token)
{
  uint8_t* handle = nullptr;
  uint8_t* suffix = nullptr;
  YamlMark start_mark, end_mark;

  start_mark = this->mark;

  // Check if the tag is in the canonical form.
  if (!this->Cache(2)) goto error;

  if (this->buffer.CheckAt('<', 1))
  {
    // Set the handle to ''
    handle = (decltype(handle))this->Malloc(1);
    if (!handle) goto error;
    handle[0] = '\0';

    // Eat '!<'
    this->Skip();
    this->Skip();

    // Consume the tag value.
    if (!this->ScanTagUri(0, nullptr, start_mark, &suffix)) goto error;

    // Check for '>' and eat it.
    if (!this->buffer.CheckAt('>'))
    {
      this->SetScannerError("while scanning a tag", start_mark, "did not find the expected '>'");
      goto error;
    }

    this->Skip();
  }
  else
  {
    // The tag has either the '!suffix' or the '!handle!suffix' form.

    // First, try to scan a handle.
    if (!this->ScanTagHandle(0, start_mark, &handle)) goto error;

    // Check if it is, indeed, handle.
    if (handle[0] == '!' && handle[1] != '\0' && handle[strlen((char*)handle) - 1] == '!')
    {
      // Scan the suffix now.
      if (!this->ScanTagUri(0, nullptr, start_mark, &suffix)) goto error;
    }
    else
    {
      // It wasn't a handle after all.  Scan the rest of the tag.
      if (!this->ScanTagUri(0, handle, start_mark, &suffix)) goto error;

      // Set the handle to '!'.
      this->Free(handle);
      handle = (decltype(handle))this->Malloc(2);
      if (!handle) goto error;
      handle[0] = '!';
      handle[1] = '\0';

      /*
       * A special case: the '!' tag.  Set the handle to '' and the
       * suffix to '!'.
       */
      if (suffix[0] == '\0')
      {
        uint8_t* tmp = handle;
        handle       = suffix;
        suffix       = tmp;
      }
    }
  }

  // Check the character which ends the tag.
  if (!this->Cache(1)) goto error;

  if (!this->buffer.IsBlankOrNulAt())
  {
    this->SetScannerError("while scanning a tag", start_mark,
                          "did not find expected whitespace or line break");
    goto error;
  }

  end_mark = this->mark;

  // Create a token.
  token = YamlToken::InitTag(handle, suffix, start_mark, end_mark);

  return true;

error:
  this->Free(handle);
  this->Free(suffix);
  return false;
}

/*
 * Scan a tag handle.
 */
bool YamlParser::ScanTagHandle(bool directive, YamlMark start_mark, uint8_t** handle)
{
  YamlString string;

  if (!string.Init(*this, INITIAL_STRING_SIZE)) goto error;

  // Check the initial '!' character.
  if (!this->Cache(1)) goto error;

  if (!this->buffer.CheckAt('!'))
  {
    this->SetScannerError(directive ? "while scanning a tag directive" : "while scanning a tag",
                          start_mark, "did not find expected '!'");
    goto error;
  }

  // Copy the '!' character.
  if (!this->Read(string)) goto error;

  // Copy all subsequent alphabetical and numerical characters.
  if (!this->Cache(1)) goto error;

  while (this->buffer.IsAlphaAt())
  {
    if (!this->Read(string)) goto error;
    if (!this->Cache(1)) goto error;
  }

  // Check if the trailing character is '!' and copy it.
  if (this->buffer.CheckAt('!'))
  {
    if (!this->Read(string)) goto error;
  }
  else
  {
    /*
     * It's either the '!' tag or not really a tag handle.  If it's a %TAG
     * directive, it's an error.  If it's a tag token, it must be a part of
     * URI.
     */
    if (directive && !(string.start[0] == '!' && string.start[1] == '\0'))
    {
      this->SetScannerError("while parsing a tag directive", start_mark,
                            "did not find expected '!'");
      goto error;
    }
  }

  *handle = string.start;

  return true;

error:
  string.Del(*this);
  return false;
}

/*
 * Scan a tag.
 */
bool YamlParser::ScanTagUri(bool directive, uint8_t* head, YamlMark start_mark, uint8_t** uri)
{
  size_t length = head ? strlen((char*)head) : 0;
  YamlString string;

  if (!string.Init(*this, INITIAL_STRING_SIZE)) goto error;

  // Resize the string to include the head.
  while ((size_t)(string.end - string.start) <= length)
  {
    if (!this->ExtendString(string))
    {
      this->error = EYamlError::Memory;
      goto error;
    }
  }

  /*
   * Copy the head if needed.
   *
   * Note that we don't copy the leading '!' character.
   */
  if (length > 1)
  {
    memcpy(string.start, head + 1, length - 1);
    string.pointer += length - 1;
  }

  // Scan the tag.
  if (!this->Cache(1)) goto error;

  /*
   * The set of characters that may appear in URI is as follows:
   *
   *      '0'-'9', 'A'-'Z', 'a'-'z', '_', '-', ';', '/', '?', ':', '@', '&',
   *      '=', '+', '$', ',', '.', '!', '~', '*', '\'', '(', ')', '[', ']',
   *      '%'.
   */
  while (this->buffer.IsAlphaAt() || this->buffer.CheckAt(';') || this->buffer.CheckAt('/') ||
         this->buffer.CheckAt('?') || this->buffer.CheckAt(':') || this->buffer.CheckAt('@') ||
         this->buffer.CheckAt('&') || this->buffer.CheckAt('=') || this->buffer.CheckAt('+') ||
         this->buffer.CheckAt('$') || this->buffer.CheckAt(',') || this->buffer.CheckAt('.') ||
         this->buffer.CheckAt('!') || this->buffer.CheckAt('~') || this->buffer.CheckAt('*') ||
         this->buffer.CheckAt('\'') || this->buffer.CheckAt('(') || this->buffer.CheckAt(')') ||
         this->buffer.CheckAt('[') || this->buffer.CheckAt(']') || this->buffer.CheckAt('%'))
  {
    // Check if it is a URI-escape sequence.
    if (this->buffer.CheckAt('%'))
    {
      if (!string.Extend(*this)) goto error;

      if (!this->ScanUriEscapes(directive, start_mark, &string)) goto error;
    }
    else
    {
      if (!this->Read(string)) goto error;
    }

    length++;
    if (!this->Cache(1)) goto error;
  }

  // Check if the tag is non-empty.
  if (!length)
  {
    if (!string.Extend(*this)) goto error;

    this->SetScannerError(directive ? "while parsing a %TAG directive" : "while parsing a tag",
                          start_mark, "did not find expected tag URI");
    goto error;
  }

  *uri = string.start;

  return true;

error:
  string.Del(*this);
  return false;
}

/*
 * Decode an URI-escape sequence corresponding to a single UTF-8 character.
 */
bool YamlParser::ScanUriEscapes(bool directive, YamlMark start_mark, YamlString* string)
{
  int width = 0;

  // Decode the required number of characters.
  do
  {
    unsigned char octet = 0;

    // Check for a URI-escaped octet.
    if (!this->Cache(3))
    {
      return false;
    }

    if (!(this->buffer.CheckAt('%') && this->buffer.IsHexAt(1) && this->buffer.IsHexAt(2)))
    {
      return this->SetScannerError(directive ? "while parsing a %TAG directive"
                                             : "while parsing a tag",
                                   start_mark, "did not find URI escaped octet");
    }

    // Get the octet.
    octet = (this->buffer.AsHexAt(1) << 4) + this->buffer.AsHexAt(2);

    // If it is the leading octet, determine the length of the UTF-8 sequence.
    if (!width)
    {
      width = (octet & 0x80) == 0x00
                  ? 1
                  : (octet & 0xE0) == 0xC0
                        ? 2
                        : (octet & 0xF0) == 0xE0 ? 3 : (octet & 0xF8) == 0xF0 ? 4 : 0;
      if (!width)
      {
        return this->SetScannerError(directive ? "while parsing a %TAG directive"
                                               : "while parsing a tag",
                                     start_mark, "found an incorrect leading UTF-8 octet");
      }
    }
    else
    {
      // Check if the trailing octet is correct.
      if ((octet & 0xC0) != 0x80)
      {
        return this->SetScannerError(directive ? "while parsing a %TAG directive"
                                               : "while parsing a tag",
                                     start_mark, "found an incorrect trailing UTF-8 octet");
      }
    }

    // Copy the octet and move the pointers.
    *(string->pointer++) = octet;
    this->Skip();
    this->Skip();
    this->Skip();

  } while (--width);

  return true;
}

/*
 * Scan a block scalar.
 */
bool YamlParser::ScanBlockScalar(YamlToken& token, bool literal)
{
  YamlMark start_mark;
  YamlMark end_mark;
  YamlString string;
  YamlString leading_break;
  YamlString trailing_breaks;
  int chomping        = 0;
  int increment       = 0;
  int indent          = 0;
  bool leading_blank  = 0;
  bool trailing_blank = 0;

  if (!string.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!leading_break.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!trailing_breaks.Init(*this, INITIAL_STRING_SIZE)) goto error;

  // Eat the indicator '|' or '>'.
  start_mark = this->mark;

  this->Skip();

  // Scan the additional block scalar indicators.
  if (!this->Cache(1)) goto error;

  // Check for a chomping indicator.
  if (this->buffer.CheckAt('+') || this->buffer.CheckAt('-'))
  {
    // Set the chomping method and eat the indicator.
    chomping = this->buffer.CheckAt('+') ? +1 : -1;

    this->Skip();

    // Check for an indentation indicator.
    if (!this->Cache(1)) goto error;

    if (this->buffer.IsDigitAt())
    {
      // Check that the indentation is greater than 0.
      if (this->buffer.CheckAt('0'))
      {
        this->SetScannerError("while scanning a block scalar", start_mark,
                              "found an indentation indicator equal to 0");
        goto error;
      }

      // Get the indentation level and eat the indicator.
      increment = this->buffer.AsDigitAt();

      this->Skip();
    }
  }

  // Do the same as above, but in the opposite order.
  else if (this->buffer.IsDigitAt())
  {
    if (this->buffer.CheckAt('0'))
    {
      this->SetScannerError("while scanning a block scalar", start_mark,
                            "found an indentation indicator equal to 0");
      goto error;
    }

    increment = this->buffer.AsDigitAt();

    this->Skip();

    if (!this->Cache(1)) goto error;

    if (this->buffer.CheckAt('+') || this->buffer.CheckAt('-'))
    {
      chomping = this->buffer.CheckAt('+') ? +1 : -1;

      this->Skip();
    }
  }

  // Eat whitespaces and comments to the end of the line.
  if (!this->Cache(1)) goto error;

  while (this->buffer.IsBlankAt())
  {
    this->Skip();
    if (!this->Cache(1)) goto error;
  }

  if (this->buffer.CheckAt('#'))
  {
    while (!this->buffer.IsBreakOrNulAt())
    {
      this->Skip();
      if (!this->Cache(1)) goto error;
    }
  }

  // Check if we are at the end of the line.
  if (!this->buffer.IsBreakOrNulAt())
  {
    this->SetScannerError("while scanning a block scalar", start_mark,
                          "did not find expected comment or line break");
    goto error;
  }

  // Eat a line break.
  if (this->buffer.IsBreakAt())
  {
    if (!this->Cache(2)) goto error;
    this->SkipLine();
  }

  end_mark = this->mark;

  // Set the indentation level if it was specified.
  if (increment)
  {
    indent = this->indent >= 0 ? this->indent + increment : increment;
  }

  // Scan the leading line breaks and determine the indentation level if needed.
  if (!this->ScanBlockScalarBreaks(&indent, &trailing_breaks, start_mark, &end_mark)) goto error;

  // Scan the block scalar content.
  if (!this->Cache(1)) goto error;

  while ((int)this->mark.column == indent && !(this->buffer.IsNulAt()))
  {
    /*
     * We are at the beginning of a non-empty line.
     */

    // Is it a trailing whitespace?
    trailing_blank = this->buffer.IsBlankAt();

    // Check if we need to fold the leading line break.
    if (!literal && (*leading_break.start == '\n') && !leading_blank && !trailing_blank)
    {
      // Do we need to join the lines by space?
      if (*trailing_breaks.start == '\0')
      {
        if (!string.Extend(*this)) goto error;
        *(string.pointer++) = ' ';
      }

      leading_break.Clear();
    }
    else
    {
      if (!string.Join(*this, leading_break)) goto error;
      leading_break.Clear();
    }

    // Append the remaining line breaks.
    if (!string.Join(*this, trailing_breaks)) goto error;
    trailing_breaks.Clear();

    // Is it a leading whitespace?
    leading_blank = this->buffer.IsBlankAt();

    // Consume the current line.
    while (!this->buffer.IsBreakOrNulAt())
    {
      if (!this->Read(string)) goto error;
      if (!this->Cache(1)) goto error;
    }

    // Consume the line break.
    if (!this->Cache(2)) goto error;

    if (!this->ReadLine(leading_break)) goto error;

    // Eat the following indentation spaces and line breaks.
    if (!this->ScanBlockScalarBreaks(&indent, &trailing_breaks, start_mark, &end_mark)) goto error;
  }

  // Chomp the tail.
  if (chomping != -1)
  {
    if (!string.Join(*this, leading_break)) goto error;
  }
  if (chomping == 1)
  {
    if (!string.Join(*this, trailing_breaks)) goto error;
  }

  // Create a token.
  token = YamlToken::InitScalar(string.start, string.pointer - string.start,
                                literal ? EYamlScalarStyle::Literal : EYamlScalarStyle::Folded,
                                start_mark, end_mark);

  leading_break.Del(*this);
  trailing_breaks.Del(*this);

  return true;

error:
  string.Del(*this);
  leading_break.Del(*this);
  trailing_breaks.Del(*this);

  return false;
}

/*
 * Scan indentation spaces and line breaks for a block scalar.  Determine the
 * indentation level if needed.
 */
bool YamlParser::ScanBlockScalarBreaks(int* indent, YamlString* breaks, YamlMark start_mark,
                                       YamlMark* end_mark)
{
  int max_indent = 0;

  *end_mark = this->mark;

  // Eat the indentation spaces and line breaks.
  while (1)
  {
    // Eat the indentation spaces.
    if (!this->Cache(1))
    {
      return false;
    }

    while ((!*indent || (int)this->mark.column < *indent) && this->buffer.IsSpaceAt())
    {
      this->Skip();
      if (!this->Cache(1))
      {
        return false;
      }
    }

    if ((int)this->mark.column > max_indent) max_indent = (int)this->mark.column;

    // Check for a tab character messing the indentation.
    if ((!*indent || (int)this->mark.column < *indent) && this->buffer.IsTabAt())
    {
      return this->SetScannerError("while scanning a block scalar", start_mark,
                                   "found a tab character where an indentation space is expected");
    }

    // Have we found a non-empty line?
    if (!this->buffer.IsBreakAt()) break;

    // Consume the line break.
    if (!this->Cache(2))
    {
      return false;
    }
    if (!this->ReadLine(*breaks))
    {
      return false;
    }
    *end_mark = this->mark;
  }

  // Determine the indentation level if needed.
  if (!*indent)
  {
    *indent = max_indent;
    if (*indent < this->indent + 1) *indent = this->indent + 1;
    if (*indent < 1) *indent = 1;
  }

  return true;
}

/*
 * Scan a quoted scalar.
 */
bool YamlParser::ScanFlowScalar(YamlToken& token, bool single)
{
  YamlMark start_mark;
  YamlMark end_mark;
  YamlString string;
  YamlString leading_break;
  YamlString trailing_breaks;
  YamlString whitespaces;
  bool leading_blanks;

  if (!string.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!leading_break.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!trailing_breaks.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!whitespaces.Init(*this, INITIAL_STRING_SIZE)) goto error;

  // Eat the left quote.
  start_mark = this->mark;

  this->Skip();

  // Consume the content of the quoted scalar.
  while (1)
  {
    // Check that there are no document indicators at the beginning of the line.
    if (!this->Cache(4)) goto error;

    if (this->mark.column == 0 &&
        ((this->buffer.CheckAt('-', 0) && this->buffer.CheckAt('-', 1) &&
          this->buffer.CheckAt('-', 2)) ||
         (this->buffer.CheckAt('.', 0) && this->buffer.CheckAt('.', 1) &&
          this->buffer.CheckAt('.', 2))) &&
        this->buffer.IsBlankOrNulAt(3))
    {
      this->SetScannerError("while scanning a quoted scalar", start_mark,
                            "found unexpected document indicator");
      goto error;
    }

    // Check for EOF.
    if (this->buffer.IsNulAt())
    {
      this->SetScannerError("while scanning a quoted scalar", start_mark,
                            "found unexpected end of stream");
      goto error;
    }

    // Consume non-blank characters.
    if (!this->Cache(2)) goto error;

    leading_blanks = false;

    while (!this->buffer.IsBlankOrNulAt())
    {
      // Check for an escaped single quote.
      if (single && this->buffer.CheckAt('\'', 0) && this->buffer.CheckAt('\'', 1))
      {
        if (!string.Extend(*this)) goto error;
        *(string.pointer++) = '\'';
        this->Skip();
        this->Skip();
      }

      // Check for the right quote.
      else if (this->buffer.CheckAt(single ? '\'' : '"'))
      {
        break;
      }

      // Check for an escaped line break.
      else if (!single && this->buffer.CheckAt('\\') && this->buffer.IsBreakAt(1))
      {
        if (!this->Cache(3)) goto error;
        this->Skip();
        this->SkipLine();
        leading_blanks = true;
        break;
      }

      // Check for an escape sequence.
      else if (!single && this->buffer.CheckAt('\\'))
      {
        size_t code_length = 0;

        if (!string.Extend(*this)) goto error;

        // Check the escape character.
        switch (this->buffer.pointer[1])
        {
        case '0':
          *(string.pointer++) = '\0';
          break;

        case 'a':
          *(string.pointer++) = '\x07';
          break;

        case 'b':
          *(string.pointer++) = '\x08';
          break;

        case 't':
        case '\t':
          *(string.pointer++) = '\x09';
          break;

        case 'n':
          *(string.pointer++) = '\x0A';
          break;

        case 'v':
          *(string.pointer++) = '\x0B';
          break;

        case 'f':
          *(string.pointer++) = '\x0C';
          break;

        case 'r':
          *(string.pointer++) = '\x0D';
          break;

        case 'e':
          *(string.pointer++) = '\x1B';
          break;

        case ' ':
          *(string.pointer++) = '\x20';
          break;

        case '"':
          *(string.pointer++) = '"';
          break;

        case '/':
          *(string.pointer++) = '/';
          break;

        case '\\':
          *(string.pointer++) = '\\';
          break;

        case 'N': // NEL (#x85)
          *(string.pointer++) = '\xC2';
          *(string.pointer++) = '\x85';
          break;

        case '_': // #xA0
          *(string.pointer++) = '\xC2';
          *(string.pointer++) = '\xA0';
          break;

        case 'L': // LS (#x2028)
          *(string.pointer++) = '\xE2';
          *(string.pointer++) = '\x80';
          *(string.pointer++) = '\xA8';
          break;

        case 'P': // PS (#x2029)
          *(string.pointer++) = '\xE2';
          *(string.pointer++) = '\x80';
          *(string.pointer++) = '\xA9';
          break;

        case 'x':
          code_length = 2;
          break;

        case 'u':
          code_length = 4;
          break;

        case 'U':
          code_length = 8;
          break;

        default:
          this->SetScannerError("while parsing a quoted scalar", start_mark,
                                "found unknown escape character");
          goto error;
        }

        this->Skip();
        this->Skip();

        // Consume an arbitrary escape code.
        if (code_length)
        {
          unsigned int value = 0;
          size_t k;

          // Scan the character value.
          if (!this->Cache(code_length)) goto error;

          for (k = 0; k < code_length; k++)
          {
            if (!this->buffer.IsHexAt(k))
            {
              this->SetScannerError("while parsing a quoted scalar", start_mark,
                                    "did not find expected hexdecimal number");
              goto error;
            }
            value = (value << 4) + this->buffer.AsHexAt(k);
          }

          // Check the value and write the character.
          if ((value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF)
          {
            this->SetScannerError("while parsing a quoted scalar", start_mark,
                                  "found invalid Unicode character escape code");
            goto error;
          }

          if (value <= 0x7F)
          {
            *(string.pointer++) = value;
          }
          else if (value <= 0x7FF)
          {
            *(string.pointer++) = 0xC0 + (value >> 6);
            *(string.pointer++) = 0x80 + (value & 0x3F);
          }
          else if (value <= 0xFFFF)
          {
            *(string.pointer++) = 0xE0 + (value >> 12);
            *(string.pointer++) = 0x80 + ((value >> 6) & 0x3F);
            *(string.pointer++) = 0x80 + (value & 0x3F);
          }
          else
          {
            *(string.pointer++) = 0xF0 + (value >> 18);
            *(string.pointer++) = 0x80 + ((value >> 12) & 0x3F);
            *(string.pointer++) = 0x80 + ((value >> 6) & 0x3F);
            *(string.pointer++) = 0x80 + (value & 0x3F);
          }

          // Advance the pointer.
          for (k = 0; k < code_length; k++)
          {
            this->Skip();
          }
        }
      }

      else
      {
        // It is a non-escaped non-blank character.
        if (!this->Read(string)) goto error;
      }

      if (!this->Cache(2)) goto error;
    }

    // Check if we are at the end of the scalar.

    /* Fix for crash unitialized value crash
     * Credit for the bug and input is to OSS Fuzz
     * Credit for the fix to Alex Gaynor
     */
    if (!this->Cache(1)) goto error;
    if (this->buffer.CheckAt(single ? '\'' : '"')) break;

    // Consume blank characters.
    if (!this->Cache(1)) goto error;

    while (this->buffer.IsBlankAt() || this->buffer.IsBreakAt())
    {
      if (this->buffer.IsBlankAt())
      {
        // Consume a space or a tab character.
        if (!leading_blanks)
        {
          if (!this->Read(whitespaces)) goto error;
        }
        else
        {
          this->Skip();
        }
      }
      else
      {
        if (!this->Cache(2)) goto error;

        // Check if it is a first line break.
        if (!leading_blanks)
        {
          whitespaces.Clear();
          if (!this->ReadLine(leading_break)) goto error;
          leading_blanks = true;
        }
        else
        {
          if (!this->ReadLine(trailing_breaks)) goto error;
        }
      }
      if (!this->Cache(1)) goto error;
    }

    // Join the whitespaces or fold line breaks.
    if (leading_blanks)
    {
      // Do we need to fold line breaks?
      if (leading_break.start[0] == '\n')
      {
        if (trailing_breaks.start[0] == '\0')
        {
          if (!string.Extend(*this)) goto error;
          *(string.pointer++) = ' ';
        }
        else
        {
          if (!string.Join(*this, trailing_breaks)) goto error;
          trailing_breaks.Clear();
        }
        leading_break.Clear();
      }
      else
      {
        if (!string.Join(*this, leading_break)) goto error;
        if (!string.Join(*this, trailing_breaks)) goto error;
        leading_break.Clear();
        trailing_breaks.Clear();
      }
    }
    else
    {
      if (!string.Join(*this, whitespaces)) goto error;
      whitespaces.Clear();
    }
  }

  // Eat the right quote.
  this->Skip();

  end_mark = this->mark;

  // Create a token.
  token = YamlToken::InitScalar(string.start, string.pointer - string.start,
                                single ? EYamlScalarStyle::SingleQuoted
                                       : EYamlScalarStyle::DoubleQuoted,
                                start_mark, end_mark);

  leading_break.Del(*this);
  trailing_breaks.Del(*this);
  whitespaces.Del(*this);

  return true;

error:
  string.Del(*this);
  leading_break.Del(*this);
  trailing_breaks.Del(*this);
  whitespaces.Del(*this);

  return false;
}

/*
 * Scan a plain scalar.
 */
bool YamlParser::ScanPlainScalar(YamlToken& token)
{
  YamlMark start_mark;
  YamlMark end_mark;
  YamlString string;
  YamlString leading_break;
  YamlString trailing_breaks;
  YamlString whitespaces;
  bool leading_blanks = false;
  int indent          = this->indent + 1;

  if (!string.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!leading_break.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!trailing_breaks.Init(*this, INITIAL_STRING_SIZE)) goto error;
  if (!whitespaces.Init(*this, INITIAL_STRING_SIZE)) goto error;

  start_mark = end_mark = this->mark;

  // Consume the content of the plain scalar.
  while (1)
  {
    // Check for a document indicator.
    if (!this->Cache(4)) goto error;

    if (this->mark.column == 0 &&
        ((this->buffer.CheckAt('-', 0) && this->buffer.CheckAt('-', 1) &&
          this->buffer.CheckAt('-', 2)) ||
         (this->buffer.CheckAt('.', 0) && this->buffer.CheckAt('.', 1) &&
          this->buffer.CheckAt('.', 2))) &&
        this->buffer.IsBlankOrNulAt(3))
      break;

    // Check for a comment.
    if (this->buffer.CheckAt('#')) break;

    // Consume non-blank characters.
    while (!this->buffer.IsBlankOrNulAt())
    {
      /* Check for "x:" + one of ',?[]{}' in the flow context. TODO: Fix the test "spec-08-13".
       * This is not completely according to the spec
       * See http://yaml.org/spec/1.1/#id907281 9.1.3. Plain
       */
      if (this->flow_level && this->buffer.CheckAt(':') &&
          (this->buffer.CheckAt(',', 1) || this->buffer.CheckAt('?', 1) ||
           this->buffer.CheckAt('[', 1) || this->buffer.CheckAt(']', 1) ||
           this->buffer.CheckAt('{', 1) || this->buffer.CheckAt('}', 1)))
      {
        this->SetScannerError("while scanning a plain scalar", start_mark, "found unexpected ':'");
        goto error;
      }

      // Check for indicators that may end a plain scalar.
      if ((this->buffer.CheckAt(':') && this->buffer.IsBlankOrNulAt(1)) ||
          (this->flow_level &&
           (this->buffer.CheckAt(',') || this->buffer.CheckAt('?') || this->buffer.CheckAt('[') ||
            this->buffer.CheckAt(']') || this->buffer.CheckAt('{') || this->buffer.CheckAt('}'))))
        break;

      // Check if we need to join whitespaces and breaks.
      if (leading_blanks || whitespaces.start != whitespaces.pointer)
      {
        if (leading_blanks)
        {
          // Do we need to fold line breaks?
          if (leading_break.start[0] == '\n')
          {
            if (trailing_breaks.start[0] == '\0')
            {
              if (!string.Extend(*this)) goto error;
              *(string.pointer++) = ' ';
            }
            else
            {
              if (!string.Join(*this, trailing_breaks)) goto error;
              trailing_breaks.Clear();
            }
            leading_break.Clear();
          }
          else
          {
            if (!string.Join(*this, leading_break)) goto error;
            if (!string.Join(*this, trailing_breaks)) goto error;
            leading_break.Clear();
            trailing_breaks.Clear();
          }

          leading_blanks = false;
        }
        else
        {
          if (!string.Join(*this, whitespaces)) goto error;
          whitespaces.Clear();
        }
      }

      // Copy the character.
      if (!this->Read(string)) goto error;

      end_mark = this->mark;

      if (!this->Cache(2)) goto error;
    }

    // Is it the end?
    if (!(this->buffer.IsBlankAt() || this->buffer.IsBreakAt())) break;

    // Consume blank characters.
    if (!this->Cache(1)) goto error;

    while (this->buffer.IsBlankAt() || this->buffer.IsBreakAt())
    {
      if (this->buffer.IsBlankAt())
      {
        // Check for tab character that abuse indentation.
        if (leading_blanks && (int)this->mark.column < indent && this->buffer.IsTabAt())
        {
          this->SetScannerError("while scanning a plain scalar", start_mark,
                                "found a tab character that violate indentation");
          goto error;
        }

        // Consume a space or a tab character.
        if (!leading_blanks)
        {
          if (!this->Read(whitespaces)) goto error;
        }
        else
        {
          this->Skip();
        }
      }
      else
      {
        if (!this->Cache(2)) goto error;

        // Check if it is a first line break.
        if (!leading_blanks)
        {
          whitespaces.Clear();
          if (!this->ReadLine(leading_break)) goto error;
          leading_blanks = true;
        }
        else
        {
          if (!this->ReadLine(trailing_breaks)) goto error;
        }
      }
      if (!this->Cache(1)) goto error;
    }

    // Check indentation level.
    if (!this->flow_level && (int)this->mark.column < indent) break;
  }

  // Create a token.
  token = YamlToken::InitScalar(string.start, string.pointer - string.start,
                                EYamlScalarStyle::Plain, start_mark, end_mark);

  // Note that we change the 'simple_key_allowed' flag.
  if (leading_blanks)
  {
    this->simple_key_allowed = true;
  }

  leading_break.Del(*this);
  trailing_breaks.Del(*this);
  whitespaces.Del(*this);

  return true;

error:
  string.Del(*this);
  leading_break.Del(*this);
  trailing_breaks.Del(*this);
  whitespaces.Del(*this);

  return false;
}

// YamlEvent

void YamlEvent::Init(EYamlEventType type, const YamlMark& start_mark, const YamlMark& end_mark)
{
  *this            = {};
  this->type       = type;
  this->start_mark = start_mark;
  this->end_mark   = end_mark;
}

void YamlEvent::InitStreamStart(EYamlEncoding encoding, const YamlMark& start_mark,
                                const YamlMark& end_mark)
{
  this->Init(EYamlEventType::StreamStart, start_mark, end_mark);
  std::get<YamlEvent::stream_start_t>(this->data).encoding = encoding;
}

void YamlEvent::InitStreamEnd(const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::StreamEnd, start_mark, end_mark);
}

void YamlEvent::InitDocumentStart(YamlVersionDirective version_directive,
                                  YamlTagDirective* tag_directives_start,
                                  YamlTagDirective* tag_directives_end, bool implicit,
                                  const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::DocumentStart, start_mark, end_mark);

  // this->data.document_start.version_directive    = version_directive;
  std::get<YamlEvent::document_start_t>(this->data).tag_directives.start = tag_directives_start;
  std::get<YamlEvent::document_start_t>(this->data).tag_directives.end   = tag_directives_end;
  std::get<YamlEvent::document_start_t>(this->data).implicit             = implicit;
}

void YamlEvent::InitDocumentEnd(bool implicit, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::DocumentEnd, start_mark, end_mark);
  std::get<YamlEvent::document_end_t>(this->data).implicit = implicit;
}

void YamlEvent::InitAlias(uint8_t* anchor, const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::Alias, start_mark, end_mark);
  std::get<YamlEvent::alias_t>(this->data).anchor = anchor;
}

void YamlEvent::InitScalar(uint8_t* anchor, uint8_t* tag, uint8_t* value, size_t length,
                           bool plain_implicit, bool quoted_implicit, EYamlScalarStyle style,
                           const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::Scalar, start_mark, end_mark);
  std::get<YamlEvent::scalar_t>(this->data).anchor          = anchor;
  std::get<YamlEvent::scalar_t>(this->data).tag             = tag;
  std::get<YamlEvent::scalar_t>(this->data).value           = value;
  std::get<YamlEvent::scalar_t>(this->data).length          = length;
  std::get<YamlEvent::scalar_t>(this->data).plain_implicit  = plain_implicit;
  std::get<YamlEvent::scalar_t>(this->data).quoted_implicit = quoted_implicit;
  std::get<YamlEvent::scalar_t>(this->data).style           = style;
}

void YamlEvent::InitSequenceStart(uint8_t* anchor, uint8_t* tag, bool implicit,
                                  EYamlSequenceStyle style, const YamlMark& start_mark,
                                  const YamlMark& end_mark)
{
  this->Init(EYamlEventType::SequenceStart, start_mark, end_mark);
  std::get<YamlEvent::sequence_start_t>(this->data).anchor   = anchor;
  std::get<YamlEvent::sequence_start_t>(this->data).tag      = tag;
  std::get<YamlEvent::sequence_start_t>(this->data).implicit = implicit;
  std::get<YamlEvent::sequence_start_t>(this->data).style    = style;
}

void YamlEvent::InitSequenceEnd(const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::SequenceEnd, start_mark, end_mark);
}

void YamlEvent::InitMappingStart(uint8_t* anchor, uint8_t* tag, bool implicit,
                                 EYamlMappingStyle style, const YamlMark& start_mark,
                                 const YamlMark& end_mark)
{
  this->Init(EYamlEventType::MappingStart, start_mark, end_mark);
  std::get<YamlEvent::mapping_start_t>(this->data).anchor   = anchor;
  std::get<YamlEvent::mapping_start_t>(this->data).tag      = tag;
  std::get<YamlEvent::mapping_start_t>(this->data).implicit = implicit;
  std::get<YamlEvent::mapping_start_t>(this->data).style    = style;
}

void YamlEvent::InitMappingEnd(const YamlMark& start_mark, const YamlMark& end_mark)
{
  this->Init(EYamlEventType::MappingEnd, start_mark, end_mark);
}

void YamlEvent::Delete(YamlParser& parser)
{
  switch (this->type)
  {
  case EYamlEventType::DocumentStart:
    parser.Free(std::get<document_start_t>(this->data).version_directive);
    for (YamlTagDirective* tag_directive =
             std::get<document_start_t>(this->data).tag_directives.start;
         tag_directive != std::get<document_start_t>(this->data).tag_directives.end;
         tag_directive++)
    {
      parser.Free(tag_directive->handle);
      parser.Free(tag_directive->prefix);
    }
    parser.Free(std::get<document_start_t>(this->data).tag_directives.start);
    break;

  case EYamlEventType::Alias:
    parser.Free(std::get<alias_t>(this->data).anchor);
    break;

  case EYamlEventType::Scalar:
    parser.Free(std::get<scalar_t>(this->data).anchor);
    parser.Free(std::get<scalar_t>(this->data).tag);
    parser.Free(std::get<scalar_t>(this->data).value);
    break;

  case EYamlEventType::SequenceStart:
    parser.Free(std::get<sequence_start_t>(this->data).anchor);
    parser.Free(std::get<sequence_start_t>(this->data).tag);
    break;

  case EYamlEventType::MappingStart:
    parser.Free(std::get<mapping_start_t>(this->data).anchor);
    parser.Free(std::get<mapping_start_t>(this->data).tag);
    break;

  default:
    break;
  }

  *this = {};
}

// YamlParser

YamlToken* YamlParser::PeekToken()
{
  return (this->token_available || this->FetchMoreTokens()) ? this->tokens.head : nullptr;
}

void YamlParser::SkipToken()
{
  this->token_available = false;
  this->tokens_parsed++;
  this->stream_end_produced = (this->tokens.head->type == EYamlTokenType::StreamEnd);
  this->tokens.head++;
}

bool YamlParser::ParseStreamStart(YamlEvent& event)
{
  YamlToken* token = this->PeekToken();

  if (!token)
  {
    return false;
  }

  if (token->type != EYamlTokenType::StreamStart)
  {
    return this->SetParserError("did not find expected <stream-start>", token->start_mark);
    return false;
  }

  this->state = EYamlParserState::ImplicitDocumentStart;

  event.InitStreamStart(std::get<YamlToken::stream_start_t>(token->data).encoding,
                        token->start_mark, token->start_mark);
  this->SkipToken();

  return true;
}

bool YamlParser::ParseDocumentStart(YamlEvent& event, bool isImplicit)
{
  return true;
}

bool YamlParser::ParseDocumentContent(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseDocumentEnd(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseNode(YamlEvent& event, bool isBlock, bool isIndentless)
{
  return true;
}

bool YamlParser::ParseBlockSequenceEntry(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseIndentlessSequenceEntry(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseBlockMappingKey(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseBlockMappingValue(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntry(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingKey(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingValue(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowSequenceEntryMappingEnd(YamlEvent& event)
{
  return true;
}

bool YamlParser::ParseFlowMappingKey(YamlEvent& event, bool isFirst)
{
  return true;
}

bool YamlParser::ParseFlowMappingValue(YamlEvent& event, bool isEmpty)
{
  return true;
}

/*
 * Generate an empty scalar event.
 */
bool YamlParser::ProcessEmptyScalar(YamlEvent& event, YamlMark mark)
{
  uint8_t* value;

  value = (decltype(value))this->Malloc(1);
  if (!value)
  {
    this->error = EYamlError::Memory;
    return false;
  }
  value[0] = '\0';

  event.InitScalar(nullptr, nullptr, value, 0, 1, 0, EYamlScalarStyle::Plain, mark, mark);

  return true;
}

/*
 * Parse directives.
 */
bool YamlParser::ProcessDirectives(YamlVersionDirective** version_directive_ref,
                                   YamlTagDirective** tag_directives_start_ref,
                                   YamlTagDirective** tag_directives_end_ref)
{
  YamlTagDirective default_tag_directives[] = {{(uint8_t*)"!", (uint8_t*)"!"},
                                               {(uint8_t*)"!!", (uint8_t*)"tag:yaml.org,2002:"},
                                               {nullptr, nullptr}};
  YamlTagDirective* default_tag_directive;
  YamlVersionDirective* version_directive = nullptr;

  YamlStack<YamlTagDirective> tag_directives;
  YamlToken* token;

  if (!tag_directives.Init(*this)) goto error;

  token = this->PeekToken();
  if (!token) goto error;

  while (token->type == EYamlTokenType::VersionDirective ||
         token->type == EYamlTokenType::TagDirective)
  {
    if (token->type == EYamlTokenType::VersionDirective)
    {
      if (version_directive)
      {
        this->SetParserError("found duplicate %YAML directive", token->start_mark);
        goto error;
      }
      if (std::get<YamlToken::version_directive_t>(token->data).major != 1 ||
          std::get<YamlToken::version_directive_t>(token->data).minor != 1)
      {
        this->SetParserError("found incompatible YAML document", token->start_mark);
        goto error;
      }
      version_directive = (decltype(version_directive))this->Malloc(sizeof(*version_directive));
      if (!version_directive)
      {
        this->error = EYamlError::Memory;
        goto error;
      }
      version_directive->major = std::get<YamlToken::version_directive_t>(token->data).major;
      version_directive->minor = std::get<YamlToken::version_directive_t>(token->data).minor;
    }

    else if (token->type == EYamlTokenType::TagDirective)
    {
      YamlTagDirective value;
      value.handle = std::get<YamlToken::tag_directive_t>(token->data).handle;
      value.prefix = std::get<YamlToken::tag_directive_t>(token->data).prefix;

      if (!this->AppendTagDirective(value, 0, token->start_mark)) goto error;
      if (!tag_directives.Push(*this, value)) goto error;
    }

    this->SkipToken();
    token = this->PeekToken();
    if (!token) goto error;
  }

  for (default_tag_directive = default_tag_directives; default_tag_directive->handle;
       default_tag_directive++)
  {
    if (!this->AppendTagDirective(*default_tag_directive, 1, token->start_mark)) goto error;
  }

  if (version_directive_ref)
  {
    *version_directive_ref = version_directive;
  }
  if (tag_directives_start_ref)
  {
    if (tag_directives.Empty())
    {
      *tag_directives_start_ref = *tag_directives_end_ref = nullptr;
      tag_directives.Del(*this);
    }
    else
    {
      *tag_directives_start_ref = tag_directives.start;
      *tag_directives_end_ref   = tag_directives.top;
    }
  }
  else
  {
    tag_directives.Del(*this);
  }

  if (!version_directive_ref) this->Free(version_directive);
  return true;

error:
  this->Free(version_directive);
  while (!tag_directives.Empty())
  {
    YamlTagDirective tag_directive = tag_directives.Pop();
    this->Free(tag_directive.handle);
    this->Free(tag_directive.prefix);
  }
  tag_directives.Del(*this);
  return false;
}

/*
 * Append a tag directive to the directives stack.
 */
bool YamlParser::AppendTagDirective(YamlTagDirective value, bool allow_duplicates, YamlMark mark)
{
  YamlTagDirective* tag_directive;
  YamlTagDirective copy = {nullptr, nullptr};

  for (tag_directive = this->tag_directives.start; tag_directive != this->tag_directives.top;
       tag_directive++)
  {
    if (strcmp((char*)value.handle, (char*)tag_directive->handle) == 0)
    {
      if (allow_duplicates) return true;
      return this->SetParserError("found duplicate %TAG directive", mark);
    }
  }

  copy.handle = (decltype(copy.prefix))this->Strdup((char*)value.handle);
  copy.prefix = (decltype(copy.prefix))this->Strdup((char*)value.prefix);
  if (!copy.handle || !copy.prefix)
  {
    this->error = EYamlError::Memory;
    goto error;
  }

  if (!this->tag_directives.Push(*this, copy)) goto error;

  return true;

error:
  this->Free(copy.handle);
  this->Free(copy.prefix);
  return false;
}

/*
 * Get the next event.
 */
bool YamlParser::Parse(YamlEvent& event)
{
  // Erase the event object.
  event = {};

  // No events after the end of the stream or error.
  if (this->stream_end_produced || (this->error != EYamlError::None) ||
      this->state == EYamlParserState::End)
  {
    return true;
  }

  // Generate the next event.
  return this->StateMachine(event);
}

/*
 * Set parser error.
 */
bool YamlParser::SetParserError(const char* problem, YamlMark problem_mark)
{
  this->error        = EYamlError::Parser;
  this->problem      = problem;
  this->problem_mark = problem_mark;

  return false;
}

bool YamlParser::SetParserErrorContext(const char* context, YamlMark context_mark,
                                       const char* problem, YamlMark problem_mark)
{
  this->error        = EYamlError::Parser;
  this->context      = context;
  this->context_mark = context_mark;
  this->problem      = problem;
  this->problem_mark = problem_mark;

  return false;
}

/*
 * String read handler.
 */
static int yaml_string_read_handler(YamlParser& parser, unsigned char* buffer, size_t size,
                                    size_t* size_read)
{
  if (parser.input.current == parser.input.end)
  {
    *size_read = 0;
    return 1;
  }

  if (size > (size_t)(parser.input.end - parser.input.current))
  {
    size = parser.input.end - parser.input.current;
  }

  memcpy(buffer, parser.input.current, size);
  parser.input.current += size;
  *size_read = size;
  return 1;
}

YamlParser::YamlParser(const YamlFns& Fns, const unsigned char* input, size_t size)
{
  assert(!this->read_handler); /* You can set the source only once. */

  this->Malloc  = Fns.Malloc;
  this->Realloc = Fns.Realloc;
  this->Free    = Fns.Free;
  this->Strdup  = Fns.Strdup;

  this->read_handler      = yaml_string_read_handler;
  this->read_handler_data = this;

  this->input.start   = input;
  this->input.current = input;
  this->input.end     = input + size;

  if (!this->raw_buffer.Init(*this, INPUT_RAW_BUFFER_SIZE)) goto error;
  if (!this->buffer.Init(*this, INPUT_BUFFER_SIZE)) goto error;
  if (!this->tokens.Init(*this, INITIAL_QUEUE_SIZE)) goto error;
  if (!this->indents.Init(*this)) goto error;
  if (!this->simple_keys.Init(*this)) goto error;
  if (!this->states.Init(*this)) goto error;
  if (!this->marks.Init(*this)) goto error;
  if (!this->tag_directives.Init(*this)) goto error;

  return;

error:

  // TODO: this is moved to the constructor but this can
  // currently fail

  this->raw_buffer.Del(*this);
  this->buffer.Del(*this);
  this->tokens.Del(*this);
  this->indents.Del(*this);
  this->simple_keys.Del(*this);
  this->states.Del(*this);
  this->marks.Del(*this);
  this->tag_directives.Del(*this);
}

/*
 * State dispatcher.
 */
bool YamlParser::StateMachine(YamlEvent& event)
{
  switch (this->state)
  {
  case EYamlParserState::StreamStart:
    return this->ParseStreamStart(event);
  case EYamlParserState::ImplicitDocumentStart:
    return this->ParseDocumentStart(event, true);
  case EYamlParserState::DocumentStart:
    return this->ParseDocumentStart(event, false);
  case EYamlParserState::DocumentContent:
    return this->ParseDocumentContent(event);
  case EYamlParserState::DocumentEnd:
    return this->ParseDocumentEnd(event);
  case EYamlParserState::BlockNode:
    return this->ParseNode(event, true, false);
  case EYamlParserState::BlockNodeOrIndentlessSequence:
    return this->ParseNode(event, true, true);
  case EYamlParserState::FlowNode:
    return this->ParseNode(event, false, false);
  case EYamlParserState::BlockSequenceFirstEntry:
    return this->ParseBlockSequenceEntry(event, true);
  case EYamlParserState::BlockSequenceEntry:
    return this->ParseBlockSequenceEntry(event, false);
  case EYamlParserState::IndentlessSequenceEntry:
    return this->ParseIndentlessSequenceEntry(event);
  case EYamlParserState::BlockMappingFirstKey:
    return this->ParseBlockMappingKey(event, true);
  case EYamlParserState::BlockMappingKey:
    return this->ParseBlockMappingKey(event, false);
  case EYamlParserState::BlockMappingValue:
    return this->ParseBlockMappingValue(event);
  case EYamlParserState::FlowSequenceFirstEntry:
    return this->ParseFlowSequenceEntry(event, true);
  case EYamlParserState::FlowSequenceEntry:
    return this->ParseFlowSequenceEntry(event, false);
  case EYamlParserState::FlowSequenceEntryMappingKey:
    return this->ParseFlowSequenceEntryMappingKey(event);
  case EYamlParserState::FlowSequenceEntryMappingValue:
    return this->ParseFlowSequenceEntryMappingValue(event);
  case EYamlParserState::FlowSequenceEntryMappingEnd:
    return this->ParseFlowSequenceEntryMappingEnd(event);
  case EYamlParserState::FlowMappingFirstKey:
    return this->ParseFlowMappingKey(event, true);
  case EYamlParserState::FlowMappingKey:
    return this->ParseFlowMappingKey(event, false);
  case EYamlParserState::FlowMappingValue:
    return this->ParseFlowMappingValue(event, false);
  case EYamlParserState::FlowMappingEmptyValue:
    return this->ParseFlowMappingValue(event, true);
  case EYamlParserState::End:
    break;
  default:
    break;
  }

  event.type = EYamlEventType::StreamEnd;
  return true;
}
