Currently, the maximum quota for a volume is 2 terabytes (2^41 bytes). Note
that this only affects the volume's quota; a volume may grow much larger if
the volume quota is disabled. However, volumes over 2 terabytes in size may
be impractical to move, and may have their size incorrectly reported by some
tools, such as L<fs_listquota(1)>.
