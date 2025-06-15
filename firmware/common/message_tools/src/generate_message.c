/* -*- c -*- */
/*
 * Copyright 2018 Cognosos, Inc.
 * Author: Sean Nowlan <sean.nowlan@cognosos.com>
 */

#include <config.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "crc16.h"
#include "lfsr.h"
#include "tpc_encoder.h"

// Print usage/help message.
void usage() {
    printf("generate_message [options]\n");
    printf("  h     : print usage\n");
    printf("  n     : number of message bytes, n >= 1, default: 10\n");
    printf("  s     : seed, s >= 1, default: 12345\n");
    printf("  c     : append CRC16, default: no\n");
    printf("  w     : whiten message, default: no\n");
}

void print_buf(const unsigned char* buf, unsigned int len) {
    fprintf(stderr, "    ");
    for (unsigned int i = 0; i < len; i++) {
        fprintf(stderr, "%02x", buf[i]);

        if (i % 2)
            fprintf(stderr, " ");

        if ((i % 16) == 7)
            fprintf(stderr, " ");

        if ((i % 16) == 15)
            fprintf(stderr, "\n    ");
    }
    fprintf(stderr, "\n\n");
}

int main(int argc, char* argv[]) {
    // options
    unsigned int msg_len = 10;     // number of message bytes
    unsigned int seed = 12345;     // RNG seed
    unsigned int crc_flag = 0;     // CRC flag
    unsigned int whiten_flag = 0;  // whiten flag

    // command line options
    int dopt;
    while ((dopt = getopt(argc, argv, "hn:s:cw")) != EOF) {
        switch (dopt) {
            case 'h':
                usage();
                return 0;
            case 'n':
                msg_len = atoi(optarg);
                break;
            case 's':
                seed = atoi(optarg);
                break;
            case 'c':
                crc_flag = 1;
                break;
            case 'w':
                whiten_flag = 1;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    // check for non-option arguments
    if (argc > optind) {
        printf("Unexpected argument: %s\n", argv[optind]);
        usage();
        exit(EXIT_FAILURE);
    }

    // validate options
    if (msg_len == 0) {
        fprintf(stderr, "error: %s, number of message bytes must be greater than zero\n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (seed == 0) {
        fprintf(stderr, "error: %s, seed value must be greater than zero\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    crc16 c = NULL;
    unsigned int crc_len = 0;

    if (crc_flag) {
        c = crc16_create_default();
        crc_len = 2;
    }

    lfsr l = NULL;
    if (whiten_flag) {
        l = lfsr_create_default();
    }

    // initialize pseudorandom number generator
    srand(seed);

    tpc_encoder encoder = tpc_encoder_create(TPC_72_40);

    unsigned int tpc_in_len = tpc_encoder_get_input_length_bytes(encoder);
    unsigned int tpc_out_len = tpc_encoder_get_output_length_bytes(encoder);

    unsigned int num_blocks = (msg_len + crc_len) / tpc_in_len;
    unsigned int pad_len = 0;

    unsigned int rem = (msg_len + crc_len) % tpc_in_len;
    if (rem > 0) {
        num_blocks++;
        pad_len = tpc_in_len - rem;
    }

    unsigned int in_len = num_blocks * tpc_in_len;
    unsigned int out_len = num_blocks * tpc_out_len;

    unsigned char* in_buf = (unsigned char*)malloc(in_len * sizeof(unsigned char));
    unsigned char* out_buf = (unsigned char*)malloc(out_len * sizeof(unsigned char));

    if (in_buf == NULL || out_buf == NULL) {
        fprintf(stderr, "Error: %s, could not allocate memory\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (unsigned int i = 0; i < msg_len; i++) {
        in_buf[i] = rand() % 256;
    }

    for (unsigned int i = msg_len; i < msg_len + pad_len; i++) {
        in_buf[i] = 0;
    }

    if (crc_flag) {
        unsigned int pay_len = msg_len + pad_len;
        crc16_reset(c);
        crc16_process(c, in_buf, pay_len);
        uint16_t checksum = crc16_checksum(c);
        in_buf[pay_len] = (checksum >> 8) & 0xff;  // upper byte
        in_buf[pay_len + 1] = checksum & 0xff;     // lower byte
    }

    fprintf(stderr, "\nORIGINAL MESSAGE:\n\n");
    print_buf(in_buf, in_len);

    if (whiten_flag) {
        lfsr_whiten_bytes(l, in_buf, in_buf, in_len);

        fprintf(stderr, "\nWHITENED MESSAGE:\n\n");
        print_buf(in_buf, in_len);
    }

    for (unsigned int i = 0; i < num_blocks; i++) {
        tpc_encoder_reset(encoder);
        tpc_encoder_encode(encoder, &in_buf[i * tpc_in_len], &out_buf[i * tpc_out_len]);
    }

    fprintf(stderr, "\nENCODED MESSAGE:\n\n");
    print_buf(out_buf, out_len);

    // clean up
    free(in_buf);
    free(out_buf);
    tpc_encoder_destroy(encoder);

    if (crc_flag) {
        crc16_destroy(c);
    }

    if (whiten_flag) {
        lfsr_destroy(l);
    }
}
