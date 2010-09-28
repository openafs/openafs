/* Internal structures that are private to RX itself. These shouldn't be
 * modified by library callers.
 *
 * Data structures that are visible to security layers, but not to
 * customers of RX belong in rx_private.h, which is installed.
 */


/* Globals that we don't want the world to know about */
extern rx_atomic_t rx_nWaiting;
extern rx_atomic_t rx_nWaited;
