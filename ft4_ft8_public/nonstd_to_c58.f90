program nonstd_to_c58
  
  integer*8 n58
  character*11 callsign
  character*38 c
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/'/

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Usage:    nonstd_to_c58 <callsign>'
     print*,'Examples: nonstd_to_c58 PJ4/K1ABC'
     print*,'          nonstd_to_c58 YW18FIFA'
     go to 999
  endif
  call getarg(1,callsign)

  n58=0
  do i=1,11
     n58=n58*38 + index(c,callsign(i:i)) - 1
  enddo
  write(*,1000) callsign,n58,n58
1000 format('Callsign: ',a11/'c58 (binary):   ' b58.58/'c58 (decimal):',i20)
  
999 end program nonstd_to_c58
