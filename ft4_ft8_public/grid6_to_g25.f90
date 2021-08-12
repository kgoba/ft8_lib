program grid6_to_g25

  parameter (MAXGRID4=32400)
  character*6 w,grid6
  character c1*1,c2*2
  logical is_grid6

  is_grid6(grid6)=len(trim(grid6)).eq.6 .and.                        &
       grid6(1:1).ge.'A' .and. grid6(1:1).le.'R' .and.               &
       grid6(2:2).ge.'A' .and. grid6(2:2).le.'R' .and.               &
       grid6(3:3).ge.'0' .and. grid6(3:3).le.'9' .and.               &
       grid6(4:4).ge.'0' .and. grid6(4:4).le.'9' .and.               &
       grid6(5:5).ge.'A' .and. grid6(5:5).le.'X' .and.               &
       grid6(6:6).ge.'A' .and. grid6(6:6).le.'X'
  
  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Convert a 6-character grid to a g25 value.'
     print*,'Usage: grid6_to_g25 IO91NP'
     go to 999
  endif
  call getarg(1,w)
  if(.not. is_grid6(w)) go to 900
  
  j1=(ichar(w(1:1))-ichar('A'))*18*10*10*24*24
  j2=(ichar(w(2:2))-ichar('A'))*10*10*24*24
  j3=(ichar(w(3:3))-ichar('0'))*10*24*24
  j4=(ichar(w(4:4))-ichar('0'))*24*24
  j5=(ichar(w(5:5))-ichar('A'))*24
  j6=(ichar(w(6:6))-ichar('A'))
  igrid6=j1+j2+j3+j4+j5+j6

  write(*,1000) w,igrid6,igrid6
1000 format('Encoded word: ',a6,'    g25 in binary: ',b25.25/     &
          30x,'decimal:',i9)
  go to 999

900 write(*,1900)
1900 format('Invalid input')

999 end program grid6_to_g25
