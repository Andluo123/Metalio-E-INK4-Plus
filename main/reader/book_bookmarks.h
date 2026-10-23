#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace reader {
// TXT: chapter/block=0, offset=raw file byte. Chapter books: original chapter,
// parsed content block and UTF-8 byte offset. Independent of page geometry.
struct BookSourcePosition { uint32_t chapter=0, block=0, offset=0; };
inline bool SourceBefore(const BookSourcePosition& a,const BookSourcePosition& b) {
    if (a.chapter!=b.chapter) return a.chapter<b.chapter;
    if (a.block!=b.block) return a.block<b.block;
    return a.offset<b.offset;
}
inline bool SourceInPage(const BookSourcePosition& point,const BookSourcePosition& first,
                         const BookSourcePosition& end) {
    return !SourceBefore(point,first) && SourceBefore(point,end);
}
struct BookBookmark {
    uint32_t id=0;
    BookSourcePosition start, end;
    uint32_t page=0;
    int64_t added_at=0;
    std::string excerpt;
};
class BookBookmarkStore {
public:
    static constexpr size_t kMaxBookmarks=256, kMaxExcerptBytes=768;
    void Load(const std::string& book_path,uint64_t file_size,int64_t mtime,uint32_t format);
    // Newest additions first; ID order is reliable even before the clock is synchronized.
    const std::vector<BookBookmark>& Entries() const { return entries_; }
    const std::string& Error() const { return error_; }
    bool Add(BookBookmark entry,std::string& error);
    bool Remove(const std::vector<uint32_t>& ids,std::string& error);
private:
    bool Save(const std::vector<BookBookmark>& entries,std::string& error);
    std::string path_,error_;
    uint64_t file_size_=0;
    int64_t mtime_=0;
    uint32_t format_=0;
    std::vector<BookBookmark> entries_;
};
} // namespace reader
