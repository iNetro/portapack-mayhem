#ifndef TURBO_DECODER_H
#define TURBO_DECODER_H

#include "Trellis.h"
#include <vector>
#include <cstring>
#include <cstdio>
#include <algorithm>

struct TurboParams {
    int k, n;
    int k_row, k_col, gk_row, gk_col;
    int b, q;
    int row_poly, col_poly;
    float scale_row;
    int num_iter;
    int m_inlen, m_outlen;
    std::vector<float> m_dec_output;
    std::vector<float> m_dec_input;
    std::vector<uint8_t> m_soft_bits;
};

class TurboDecoder {
public:
    TurboParams p;
    Trellis row_trellis, col_trellis;

    int row_size, col_size;

    std::vector<float> m_row_input;
    std::vector<float> m_row_output;
    std::vector<float> m_col_input;
    std::vector<float> m_col_output;

    TurboDecoder(const TurboParams& tp)
        : p(tp),
          row_trellis(tp.row_poly, tp.gk_row, p.k_row + p.gk_row - 1),
          col_trellis(tp.col_poly, tp.gk_col, p.k_col + p.gk_col - 1)
    {
        row_size = p.k_row + p.gk_row - 1;
        col_size = p.k_col + p.gk_col - 1;
        m_row_input.resize(row_size * col_size);
        m_row_output.resize(row_size * col_size);
        m_col_input.resize(row_size * col_size);
        m_col_output.resize(row_size * col_size);

        p.m_dec_input.resize(row_size * col_size);
        p.m_dec_output.resize(row_size * col_size);
        p.m_soft_bits.resize(p.m_outlen);
    }

    std::vector<float>& execute(const std::vector<float>& input_block) {
        // 1. Copy input ONCE at the beginning.
        std::memcpy(m_row_input.data(), input_block.data(), sizeof(float) * row_size * col_size);

        // Pointers for quick access
        float* __restrict__ row_in = m_row_input.data();
        float* __restrict__ row_out = m_row_output.data();
        float* __restrict__ col_in = m_col_input.data();
        float* __restrict__ col_out = m_col_output.data();

        const int data_len = p.k_col * row_size;
        const int block_elems = row_size * col_size;
        const float* __restrict__ input_ptr = input_block.data();

        for (int iter = 0; iter < p.num_iter; ++iter) {
            // 2. Row decoding -- avoid vector temporaries, use static buffer if possible
            for (int i = 0; i < p.k_col; ++i) {
                // Instead of returning std::vector, use buffer or pass output ptr
                const float* __restrict__ llrs = row_trellis.compute_metrics(m_row_input, row_size, i).data();
                float* __restrict__ out_ptr = row_out + i * row_size;

                // Unroll if row_size is small/fixed, otherwise let compiler vectorize
                for (int j = 0; j < row_size; ++j)
                    out_ptr[j] = llrs[j];
            }

            // 3. Soft info update: combine, scale, and add original input (fused loop)
            for (int i = 0; i < data_len; ++i)
                row_out[i] = p.scale_row * (row_out[i] + row_in[i]) + input_ptr[i];

            // 4. Copy any unused tail (padding), if needed
            if (data_len < block_elems)
                std::memcpy(row_out + data_len, row_in + data_len, sizeof(float) * (block_elems - data_len));

            // 5. Transpose to col_in (row-major, cache-friendly)
            // Can consider block transposing for large matrices, here just regular transpose
            for (int i = 0; i < col_size; ++i) {
                float* __restrict__ dst = col_in + i;
                float* __restrict__ src = row_out + i * row_size;
                for (int j = 0; j < row_size; ++j) {
                    dst[j * col_size] = src[j];
                }
            }

            // 6. Column decoding
            for (int i = 0; i < row_size; ++i) {
                const float* __restrict__ llrs = col_trellis.compute_metrics(m_col_input, col_size, i).data();
                float* __restrict__ out_ptr = col_out + i * col_size;
                for (int j = 0; j < col_size; ++j)
                    out_ptr[j] = llrs[j];
            }

            // 7. Transpose back to row_in for next iter
            for (int i = 0; i < col_size; ++i) {
                float* __restrict__ src = col_out + i;
                float* __restrict__ dst = row_in + i * row_size;
                for (int j = 0; j < row_size; ++j) {
                    dst[j] = src[j * col_size];
                }
            }
        }
        return m_row_input;
    }

    std::vector<uint8_t> extract(const std::vector<float>& block_llrs) 
    {
        static std::vector<uint8_t> bits(p.k_row * p.k_col, 0.0f);
        bits.clear();
        for (int j = p.b + p.q; j < p.k_row; ++j)
            bits.push_back(block_llrs[j] > 0.0f ? 1 : 0);
        for (int i = 1; i < p.k_col; ++i)
            for (int j = 0; j < p.k_row; ++j)
                bits.push_back(block_llrs[i * row_size + j] > 0.0f ? 1 : 0);
        return bits;
    }

    void decode(std::vector<float>& in, std::vector<uint8_t>& out, int n)
    {
        for (int i = 0; i < n; i++)
        {
            std::memcpy(p.m_dec_input.data() + p.b, in.data() + i * p.m_inlen, (p.m_inlen - p.b) * sizeof(float));

            auto& dec_out = execute(p.m_dec_input);
            p.m_dec_output = dec_out;
            p.m_soft_bits = extract(p.m_dec_output);

            for (int j = 0; j < p.m_outlen; j++)
                out[i * p.m_outlen + j] = (p.m_soft_bits[j] > 0.0f) ? 0x01 : 0x00;
        }
    }

    inline void lfsr_dewhiten(std::vector<uint8_t>& data, uint16_t seed = 0x1FF, uint16_t poly = 0x21, int order = 9) {
        uint16_t lfsr = seed;
        int bitlen = data.size();
        for (int i = 0; i < bitlen; ++i) {
            uint8_t whiten_bit = lfsr & 1;
            uint8_t data_bit = data[i] & 1;
            data[i] = (data_bit ^ whiten_bit) & 0x01;
            uint8_t feedback = 0;
            for (int b = 0; b < order; ++b) {
                if ((poly >> b) & 1) {
                    feedback ^= (lfsr >> b) & 1;
                }
            }
            lfsr = ((lfsr >> 1) | (feedback << (order - 1))) & ((1 << order) - 1);
        }
    }
};

#endif // TURBO_DECODER_H
