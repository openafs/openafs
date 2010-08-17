/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Ticket lifetime.  This defines the table used to lookup lifetime for the
   fixed part of rande of the one byte lifetime field.  Values less than 0x80
   are intrpreted as the number of 5 minute intervals.  Values from 0x80 to
   0xBF should be looked up in this table.  The value of 0x80 is the same using
   both methods: 10 and two-thirds hours .  The lifetime of 0xBF is 30 days.
   The intervening values of have a fixed ratio of roughly 1.06914.  The value
   oxFF is defined to mean a ticket has no expiration time.  This should be
   used advisedly since individual servers may impose defacto upperbounds on
   ticket lifetimes. */

#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */

static const int tkt_lifetimes[TKTLIFENUMFIXED] = {
    38400,			/* 10.67 hours, 0.44 days */
    41055,			/* 11.40 hours, 0.48 days */
    43894,			/* 12.19 hours, 0.51 days */
    46929,			/* 13.04 hours, 0.54 days */
    50174,			/* 13.94 hours, 0.58 days */
    53643,			/* 14.90 hours, 0.62 days */
    57352,			/* 15.93 hours, 0.66 days */
    61318,			/* 17.03 hours, 0.71 days */
    65558,			/* 18.21 hours, 0.76 days */
    70091,			/* 19.47 hours, 0.81 days */
    74937,			/* 20.82 hours, 0.87 days */
    80119,			/* 22.26 hours, 0.93 days */
    85658,			/* 23.79 hours, 0.99 days */
    91581,			/* 25.44 hours, 1.06 days */
    97914,			/* 27.20 hours, 1.13 days */
    104684,			/* 29.08 hours, 1.21 days */
    111922,			/* 31.09 hours, 1.30 days */
    119661,			/* 33.24 hours, 1.38 days */
    127935,			/* 35.54 hours, 1.48 days */
    136781,			/* 37.99 hours, 1.58 days */
    146239,			/* 40.62 hours, 1.69 days */
    156350,			/* 43.43 hours, 1.81 days */
    167161,			/* 46.43 hours, 1.93 days */
    178720,			/* 49.64 hours, 2.07 days */
    191077,			/* 53.08 hours, 2.21 days */
    204289,			/* 56.75 hours, 2.36 days */
    218415,			/* 60.67 hours, 2.53 days */
    233517,			/* 64.87 hours, 2.70 days */
    249664,			/* 69.35 hours, 2.89 days */
    266926,			/* 74.15 hours, 3.09 days */
    285383,			/* 79.27 hours, 3.30 days */
    305116,			/* 84.75 hours, 3.53 days */
    326213,			/* 90.61 hours, 3.78 days */
    348769,			/* 96.88 hours, 4.04 days */
    372885,			/* 103.58 hours, 4.32 days */
    398668,			/* 110.74 hours, 4.61 days */
    426234,			/* 118.40 hours, 4.93 days */
    455705,			/* 126.58 hours, 5.27 days */
    487215,			/* 135.34 hours, 5.64 days */
    520904,			/* 144.70 hours, 6.03 days */
    556921,			/* 154.70 hours, 6.45 days */
    595430,			/* 165.40 hours, 6.89 days */
    636601,			/* 176.83 hours, 7.37 days */
    680618,			/* 189.06 hours, 7.88 days */
    727680,			/* 202.13 hours, 8.42 days */
    777995,			/* 216.11 hours, 9.00 days */
    831789,			/* 231.05 hours, 9.63 days */
    889303,			/* 247.03 hours, 10.29 days */
    950794,			/* 264.11 hours, 11.00 days */
    1016537,			/* 282.37 hours, 11.77 days */
    1086825,			/* 301.90 hours, 12.58 days */
    1161973,			/* 322.77 hours, 13.45 days */
    1242318,			/* 345.09 hours, 14.38 days */
    1328218,			/* 368.95 hours, 15.37 days */
    1420057,			/* 394.46 hours, 16.44 days */
    1518247,			/* 421.74 hours, 17.57 days */
    1623226,			/* 450.90 hours, 18.79 days */
    1735464,			/* 482.07 hours, 20.09 days */
    1855462,			/* 515.41 hours, 21.48 days */
    1983758,			/* 551.04 hours, 22.96 days */
    2120925,			/* 589.15 hours, 24.55 days */
    2267576,			/* 629.88 hours, 26.25 days */
    2424367,			/* 673.44 hours, 28.06 days */
    2592000
};				/* 720.00 hours, 30.00 days */
