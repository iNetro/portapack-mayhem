#ifndef TRELLIS_H
#define TRELLIS_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>

class Trellis {
public:
    int k_length;
    int num_states;
    int poly;
    int block_size;
    std::vector<uint8_t> fw_state_table, bw_state_table;

    std::vector<float> bm;
    std::vector<float> fw;
    std::vector<float> bw;
    std::vector<float> llrs;

    Trellis(int poly, int k_len, int block_size) 
        : k_length(k_len),
         poly(poly),
         block_size(block_size)
    {
        num_states = 1 << (k_length - 1);
        fw_state_table.resize(2 * num_states);
        bw_state_table.resize(2 * num_states);
        generate_states();
    }

    void generate_states() {
        for (int state = 0; state < num_states; ++state) {
            for (int bit = 0; bit < 2; ++bit) {
                int feedback = bit ^ (__builtin_popcount(state & poly) & 1);
                int next = (state ^ (feedback << (k_length - 1))) >> 1;
                fw_state_table[2 * next + bit] = state;
                bw_state_table[2 * state + bit] = next;
            }
        }
    }

    std::vector<float> compute_metrics(const std::vector<float>& input, int size, int idx) {
        const int bm_stride = 2 * num_states;
        int offset = idx * size;

        // Flat arrays: bm[size * 2 * num_states], fw[(size+1) * num_states], bw[(size+1) * num_states]
        static std::vector<float> bm(block_size * bm_stride, 0.0f);
        static std::vector<float> fw((block_size + 1) * num_states, -INFINITY);
        static std::vector<float> bw((block_size + 1) * num_states, -INFINITY);
        static std::vector<float> llrs(block_size);

        fw[0 * num_states + 0] = 0.0f;
        bw[0 * num_states + 0] = 0.0f;

        // BM
        for (int i = 0; i < size; ++i) {
            float val = input[offset + i];
            int base = i * bm_stride;
            for (int s = 0; s < num_states; ++s) {
                bm[base + 2 * s]     = -val;
                bm[base + 2 * s + 1] =  val;
            }
        }

        // FW
        for (int i = 0; i < size; ++i) {
            int prev = i * num_states;
            int next = (i + 1) * num_states;
            int bm_off = i * bm_stride;
            for (int s = 0; s < num_states; ++s) {
                int prev0 = fw_state_table[2 * s];
                int prev1 = fw_state_table[2 * s + 1];

                float c0 = fw[prev + prev0] + bm[bm_off + 2 * prev0];
                float c1 = fw[prev + prev1] + bm[bm_off + 2 * prev1 + 1];

                fw[next + s] = (c0 > c1) ? c0 : c1;
            }
        }

        // BW
        for (int i = 0; i < size; ++i) {
            int rev_i = size - 1 - i;
            int prev = i * num_states;
            int next = (i + 1) * num_states;
            int bm_off = rev_i * bm_stride;
            for (int s = 0; s < num_states; ++s) {
                int next0 = bw_state_table[2 * s];
                int next1 = bw_state_table[2 * s + 1];

                float c0 = bw[prev + next0] + bm[bm_off + 2 * s];
                float c1 = bw[prev + next1] + bm[bm_off + 2 * s + 1];

                bw[next + s] = (c0 > c1) ? c0 : c1;
            }
        }

        for (int i = 0; i < size; ++i) {
            float lv0 = -INFINITY, lv1 = -INFINITY;
            int fw_off = i * num_states;
            int bw_off = (size - 1 - i) * num_states;
            for (int s = 0; s < num_states; ++s) {
                int ns0 = bw_state_table[2 * s];
                int ns1 = bw_state_table[2 * s + 1];

                float v0 = fw[fw_off + s] + bw[bw_off + ns0];
                float v1 = fw[fw_off + s] + bw[bw_off + ns1];
                if (v0 > lv0) lv0 = v0;
                if (v1 > lv1) lv1 = v1;
            }
            llrs[i] = lv1 - lv0;
        }

        return llrs;
    }
};

#endif // TRELLIS_H