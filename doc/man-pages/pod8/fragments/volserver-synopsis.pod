B<volserver>
    [B<-log>] S<<< [B<-p> <I<number of processes>>] >>>
    S<<< [B<-auditlog> [<I<interface>>:]<I<path>>[:<I<options>>]] >>>
    S<<< [B<-audit-interface> <I<default interface>>] >>>
    S<<< [B<-logfile <I<log file>>] >>> S<<< [B<-config> <I<configuration path>>] >>>
    S<<< [B<-udpsize> <I<size of socket buffer in bytes>>] >>>
    S<<< [B<-d> <I<debug level>>] >>>
    [B<-nojumbo>] [B<-jumbo>]
    [B<-enable_peer_stats>] [B<-enable_process_stats>]
    [B<-allow-dotted-principals>] [B<-clear-vol-stats>]
    [B<-sync> <I<sync behavior>>]
    [B<-rxmaxmtu> <I<bytes>>]
    [B<-rxbind>]
    [B<-syslog>[=<I<FACILITY>]]
    [B<-transarc-logs>]
    [B<-sleep> <I<sleep time>/I<run time>>]
    [B<-restricted_query> (anyuser | admin)]
    [B<-s2scrypt> (never | always | inherit)]
    [B<-help>]
