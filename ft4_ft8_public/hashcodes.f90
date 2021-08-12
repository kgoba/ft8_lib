program hashcodes

  parameter (NTOKENS=2063592)
  integer*8 nprime,n8(3)
  integer nbits(3),ihash(3)
  character*11 callsign
  character*38 c
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/'/
  data nprime/47055833459_8/,nbits/10,12,22/

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Usage:    hashcodes <callsign>'
     print*,'Examples: hashcodes PJ4/K1ABC'
     print*,'          hashcodes YW18FIFA'
     go to 999
  endif
  call getarg(1,callsign)
  callsign=adjustl(callsign)

  do k=1,3
     n8(k)=0
     do i=1,11
        j=index(c,callsign(i:i)) - 1
        n8(k)=38*n8(k) + j
     enddo
     ihash(k)=ishft(nprime*n8(k),nbits(k)-64)
  enddo
  ih22_biased=ihash(3) + NTOKENS
  write(*,1000) callsign,ihash,ih22_biased
1000 format('Callsign',9x,'h10',7x,'h12',7x,'h22'/41('-')/        &
          a11,i9,2i10,/'Biased for storage in c28:',i14)

999 end program hashcodes
