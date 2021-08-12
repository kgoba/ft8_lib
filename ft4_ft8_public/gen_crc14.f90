program gen_crc14

  character m77*77,c14*14

  integer mc(96),r(15),p(15),ncrc
! polynomial for 14-bit CRC 0x6757
  data p/1,1,0,0,1,1,1,0,1,0,1,0,1,1,1/
  
  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Usage:   gen_crc14 <77-bit message>'
     print*,'Example: gen_crc14 "00000000000000000000000000100000010011011111110011011100100010100001010000001"'
     go to 999
  endif

! pad the 77bit message out to 96 bits
  call getarg(1,m77)
  read(m77,'(77i1)') mc(1:77)
  mc(78:96)=0

! divide by polynomial
  r=mc(1:15)
  do i=0,81
    r(15)=mc(i+15)
    r=mod(r+r(1)*p,2)
    r=cshift(r,1)
  enddo

! the crc is in r(1:14) - print it in various ways:
  write(c14,'(14b1)') r(1:14)
  write(*,'(a40,1x,a14)') 'crc14 as a string: ',c14
  read(c14,'(b14.14)') ncrc
  write(*,'(a40,i6)') 'crc14 as an integer: ',ncrc
  write(*,'(a40,1x,b14.14)') 'binary representation of the integer: ',ncrc

999 end program gen_crc14
