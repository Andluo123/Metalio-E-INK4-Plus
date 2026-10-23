#pragma once

// Gate FatFS admissions, including third-party decoders and stdio callers.
namespace sd_access {
void Quiesce();
void Resume();
bool IsQuiescing();
bool IsIdle();
unsigned Pending();

// Only standby image decoding and controlled mount/restore may admit new work
// after Quiesce. Existing file/directory handles can always finish and close.
class Permit {
public:
    Permit();
    ~Permit();
    Permit(const Permit&) = delete;
    Permit& operator=(const Permit&) = delete;
};

// Keeps an admitted multi-file write alive through rename/cleanup after fclose.
class Transaction {
public:
    Transaction();
    ~Transaction();
    explicit operator bool() const { return admitted_; }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
private:
    bool admitted_;
};
}
