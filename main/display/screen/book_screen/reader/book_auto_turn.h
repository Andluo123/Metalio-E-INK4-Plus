#pragma once

// Timer lifecycle and request consumption run on LVGL. Manual input resets use atomics.
// Enabled state belongs to this reading session.
bool ReaderAutoTurnEnabled();
void SetReaderAutoTurnEnabled(bool enabled);
void StopReaderAutoTurn();
void ResetReaderAutoTurnDeadline();
void ReaderAutoTurnPageMoved();

// Manual input resets the deadline immediately and cancels an unconsumed auto turn.
void ReaderAutoTurnManualInput();
bool ConsumeReaderAutoTurnRequest();
