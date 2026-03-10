use std::collections::{HashMap, VecDeque};
use std::fs;
use std::hash::{Hash, Hasher};
use std::time::UNIX_EPOCH;

use vpg_core::{PreviewFrame, ProjectFile};

const DEFAULT_PREVIEW_CACHE_CAPACITY: usize = 48;

/// Cache key for decoded preview frames.
///
/// File metadata is folded into the key so edits to the source video invalidate old frames inside
/// the current app session without requiring a separate cache clear command.
#[derive(Debug, Clone, Eq)]
pub struct PreviewCacheKey {
    video_path: String,
    file_size: u64,
    modified_ms: u128,
    time_ms: u64,
    max_width: u32,
}

impl PreviewCacheKey {
    /// Creates a key for the current preview request.
    pub fn from_project(project: &ProjectFile, time_ms: u64, max_width: u32) -> Self {
        let (file_size, modified_ms) = file_metadata_stamp(&project.video.path);

        Self {
            video_path: project.video.path.clone(),
            file_size,
            modified_ms,
            time_ms,
            max_width,
        }
    }
}

impl PartialEq for PreviewCacheKey {
    fn eq(&self, other: &Self) -> bool {
        self.video_path == other.video_path
            && self.file_size == other.file_size
            && self.modified_ms == other.modified_ms
            && self.time_ms == other.time_ms
            && self.max_width == other.max_width
    }
}

impl Hash for PreviewCacheKey {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.video_path.hash(state);
        self.file_size.hash(state);
        self.modified_ms.hash(state);
        self.time_ms.hash(state);
        self.max_width.hash(state);
    }
}

/// Small bounded in-memory LRU cache for decoded preview frames.
#[derive(Debug)]
pub struct PreviewCache {
    capacity: usize,
    entries: HashMap<PreviewCacheKey, PreviewFrame>,
    usage_order: VecDeque<PreviewCacheKey>,
}

impl Default for PreviewCache {
    fn default() -> Self {
        Self::with_capacity(DEFAULT_PREVIEW_CACHE_CAPACITY)
    }
}

impl PreviewCache {
    /// Creates a cache with a fixed maximum number of preview frames.
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            capacity: capacity.max(1),
            entries: HashMap::new(),
            usage_order: VecDeque::new(),
        }
    }

    /// Returns a cached preview frame and marks it as recently used.
    pub fn get(&mut self, key: &PreviewCacheKey) -> Option<PreviewFrame> {
        let frame = self.entries.get(key)?.clone();
        self.touch(key);
        Some(frame)
    }

    /// Inserts or refreshes a cached preview frame.
    pub fn insert(&mut self, key: PreviewCacheKey, frame: PreviewFrame) {
        if self.entries.insert(key.clone(), frame).is_some() {
            self.remove_from_usage_order(&key);
        }
        self.usage_order.push_back(key);

        while self.entries.len() > self.capacity {
            if let Some(oldest_key) = self.usage_order.pop_front() {
                self.entries.remove(&oldest_key);
            }
        }
    }

    fn touch(&mut self, key: &PreviewCacheKey) {
        self.remove_from_usage_order(key);
        self.usage_order.push_back(key.clone());
    }

    fn remove_from_usage_order(&mut self, key: &PreviewCacheKey) {
        if let Some(index) = self
            .usage_order
            .iter()
            .position(|candidate| candidate == key)
        {
            self.usage_order.remove(index);
        }
    }
}

fn file_metadata_stamp(path: &str) -> (u64, u128) {
    let Ok(metadata) = fs::metadata(path) else {
        return (0, 0);
    };

    let modified_ms = metadata
        .modified()
        .ok()
        .and_then(|time| time.duration_since(UNIX_EPOCH).ok())
        .map(|duration| duration.as_millis())
        .unwrap_or(0);

    (metadata.len(), modified_ms)
}

#[cfg(test)]
mod tests {
    use vpg_core::PreviewFrame;

    use super::PreviewCache;

    fn sample_frame(label: &str) -> PreviewFrame {
        PreviewFrame {
            data_url: format!("data:image/png;base64,{label}"),
            time_ms: 1_000,
            frame_index: 24,
        }
    }

    #[test]
    fn evicts_least_recently_used_entry() {
        let mut cache = PreviewCache::with_capacity(2);
        let key_a = super::PreviewCacheKey {
            video_path: "a.mp4".to_string(),
            file_size: 1,
            modified_ms: 1,
            time_ms: 100,
            max_width: 320,
        };
        let key_b = super::PreviewCacheKey {
            video_path: "b.mp4".to_string(),
            file_size: 1,
            modified_ms: 1,
            time_ms: 200,
            max_width: 320,
        };
        let key_c = super::PreviewCacheKey {
            video_path: "c.mp4".to_string(),
            file_size: 1,
            modified_ms: 1,
            time_ms: 300,
            max_width: 320,
        };

        cache.insert(key_a.clone(), sample_frame("a"));
        cache.insert(key_b.clone(), sample_frame("b"));
        assert!(cache.get(&key_a).is_some());
        cache.insert(key_c.clone(), sample_frame("c"));

        assert!(cache.get(&key_a).is_some());
        assert!(cache.get(&key_b).is_none());
        assert!(cache.get(&key_c).is_some());
    }
}
