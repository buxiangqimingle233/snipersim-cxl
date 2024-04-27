#pragma once

#include "murmur.h"
#include <unordered_map>

// #define CHEET 1

template <typename KeyType, typename ValueType>
class CuckooHashMap {

public:
    CuckooHashMap(size_t size, size_t maxTries)
        : table(2, std::vector<std::pair<KeyType, ValueType>>(size)),
          cheetboard(std::unordered_map<KeyType, ValueType>()),
          num_buckets(size), max_attempts(maxTries), m_cnt_remove(0), m_cnt_remove_fail(0) 
    { 
        
    }

    // Returns the number of attempts made to insert
    int insert(KeyType key, ValueType value) {
#ifdef CHEET
        cheetboard[key] = value;
        return 1;
#endif
        size_t attempt;
        size_t last_selected_hash = num_buckets;
        for (attempt = 0; attempt < max_attempts; ++attempt) {
            size_t hashed_index[2];
            hash_func(key, hashed_index);
            hashed_index[0] %= num_buckets;
            hashed_index[1] %= num_buckets;

            for (size_t i = 0; i < 2; ++i) {
                size_t index = hashed_index[i];
                if (table[i][index].first == key) {
                    table[i][index].second = value;
                    return attempt + 1; // Successfully updated existing value
                }
                if (!table[i][index].first) {
                    table[i][index] = {key, value};
                    return attempt + 1; // Successfully inserted new value
                }
            }

            int bucket_to_kickout = hashed_index[0] == last_selected_hash ? 1 : 0;
            last_selected_hash = hashed_index[bucket_to_kickout];
            // Kick out the first entry if not inserted
            size_t index = hashed_index[bucket_to_kickout];
            std::swap(table[bucket_to_kickout][index].first, key);
            std::swap(table[bucket_to_kickout][index].second, value);
            std::cout << "Kicked out key: " << key << std::endl;
            // std::swap(table[i][index], std::pair<KeyType, ValueType>(key, value));
        }

        throw std::overflow_error("Hash table insertion failed after max attempts");

    }

    std::pair<KeyType, ValueType>* find(const KeyType& key) {
#ifdef CHEET
        if (cheetboard.find(key) != cheetboard.end()) {
            return &table[0][0];
        } else {
            return NULL;
        }
#endif

        size_t hashed_index[2];
        hash_func(key, hashed_index);
        hashed_index[0] %= num_buckets;
        hashed_index[1] %= num_buckets;
    
        for (size_t i = 0; i < 2; ++i) {
            if (table[i][hashed_index[i]].first == key) {
                return &table[i][hashed_index[i]];
            }
        }
        return NULL;
    }

    bool remove(const KeyType& key) {
#ifdef CHEET
        m_cnt_remove++;
        if (cheetboard.find(key) != cheetboard.end()) {
            cheetboard.erase(key);
            return true;
        } else {
            m_cnt_remove_fail++;
            return false;
        }
#endif
        std::pair<KeyType, ValueType>* entry = find(key);
        m_cnt_remove++;
        
        if (entry) {
            entry->first = KeyType();
            return true;
        }
        m_cnt_remove_fail++;
        return false;
    }

    void print_states() {
        size_t filled = 0;
        for (size_t i = 0; i < num_buckets; ++i) {
            if (table[0][i].first) {
                filled++;
            }
            if (table[1][i].first) {
                filled++;
            }
        }
        std::cout << "View Address Table Fill Ratio: " << filled << "/" << 2 * num_buckets << std::endl;
        printf("Remove: %d, Remove Fail: %d\n", m_cnt_remove, m_cnt_remove_fail);
    }

private:
    std::vector<std::vector<std::pair<KeyType, ValueType>>> table;
    std::unordered_map<KeyType, ValueType> cheetboard;
    size_t num_buckets;
    size_t max_attempts;
    int m_cnt_remove, m_cnt_remove_fail;

    void hash_func(const KeyType& key, size_t* hashed_key) const {
        size_t out[4];
        MurmurHash3_x64_128(&key, sizeof(KeyType), SALT_CONSTANT, out);
        hashed_key[0] = out[0];
        hashed_key[1] = out[1];
    }
};

