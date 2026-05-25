PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY,
    mode TEXT NOT NULL,
    title TEXT,
    started_at TEXT NOT NULL,
    ended_at TEXT,
    summary_status TEXT DEFAULT 'pending'
);

CREATE TABLE IF NOT EXISTS transcript_segments (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    start_ms INTEGER NOT NULL,
    end_ms INTEGER NOT NULL,
    speaker TEXT,
    text TEXT NOT NULL,
    source TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS screen_ocr_segments (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    window_title TEXT,
    text TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS processed_notes (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    type TEXT NOT NULL,
    title TEXT,
    body TEXT NOT NULL,
    json_body TEXT,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS action_items (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    text TEXT NOT NULL,
    status TEXT DEFAULT 'open',
    due_at TEXT,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS flashcards (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    question TEXT NOT NULL,
    answer TEXT NOT NULL,
    topic TEXT,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS model_events (
    id TEXT PRIMARY KEY,
    event_type TEXT NOT NULL,
    model_name TEXT,
    details TEXT,
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS privacy_events (
    id TEXT PRIMARY KEY,
    session_id TEXT,
    event_type TEXT NOT NULL,
    details TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_transcript_segments_session_start
    ON transcript_segments(session_id, start_ms);

CREATE INDEX IF NOT EXISTS idx_screen_ocr_segments_session_time
    ON screen_ocr_segments(session_id, timestamp_ms);

CREATE INDEX IF NOT EXISTS idx_processed_notes_session_type
    ON processed_notes(session_id, type);

CREATE INDEX IF NOT EXISTS idx_action_items_session_status
    ON action_items(session_id, status);

CREATE INDEX IF NOT EXISTS idx_flashcards_session_topic
    ON flashcards(session_id, topic);

CREATE INDEX IF NOT EXISTS idx_model_events_created_at
    ON model_events(created_at);

CREATE INDEX IF NOT EXISTS idx_privacy_events_session_created
    ON privacy_events(session_id, created_at);

CREATE INDEX IF NOT EXISTS idx_sessions_started_at
    ON sessions(started_at);

CREATE VIRTUAL TABLE IF NOT EXISTS transcript_fts
USING fts5(id UNINDEXED, session_id UNINDEXED, text);

CREATE VIRTUAL TABLE IF NOT EXISTS processed_notes_fts
USING fts5(id UNINDEXED, session_id UNINDEXED, title, body);
