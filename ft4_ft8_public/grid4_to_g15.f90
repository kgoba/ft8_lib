program grid4_to_g15

  parameter (MAXGRID4=32400)
  character*4 w,grid4
  character c1*1,c2*2
  logical is_grid4
  is_grid4(grid4)=len(trim(grid4)).eq.4 .and.                        &
       grid4(1:1).ge.'A' .and. grid4(1:1).le.'R' .and.               &
       grid4(2:2).ge.'A' .and. grid4(2:2).le.'R' .and.               &
       grid4(3:3).ge.'0' .and. grid4(3:3).le.'9' .and.               &
       grid4(4:4).ge.'0' .and. grid4(4:4).le.'9'

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Convert a 4-character grid, signal report, etc., to a g15 value.'
     print*,'Usage examples:'
     print*,'grid4_to_g15 FN20'
     print*,'grid4_to_g15 -11'
     print*,'grid4_to_g15 +02'
     print*,'grid4_to_g15 RRR'
     print*,'grid4_to_g15 RR73'
     print*,'grid4_to_g15 73'
     print*,'grid4_to_g15 ""'
     go to 999
  endif
  call getarg(1,w)
  if(is_grid4(w) .and. w.ne.'RR73') then
     j1=(ichar(w(1:1))-ichar('A'))*18*10*10
     j2=(ichar(w(2:2))-ichar('A'))*10*10
     j3=(ichar(w(3:3))-ichar('0'))*10
     j4=(ichar(w(4:4))-ichar('0'))
     igrid4=j1+j2+j3+j4     
  else
     c1=w(1:1)
     if(c1.ne.'+' .and. c1.ne.'-'.and. trim(w).ne.'RRR' .and. w.ne.'RR73' &
          .and. trim(w).ne.'73' .and. len(trim(w)).ne.0) go to 900
     if(c1.eq.'+' .or. c1.eq.'-') then
        read(w,*,err=900) irpt
        irpt=irpt+35
     endif
     if(len(trim(w)).eq.0) irpt=1
     if(trim(w).eq.'RRR') irpt=2
     if(w.eq.'RR73') irpt=3
     if(trim(w).eq.'73') irpt=4
     igrid4=MAXGRID4 + irpt
  endif

  write(*,1000) w,igrid4,igrid4
1000 format('Encoded word: ',a4,'   g15 in binary: ',b15.15,'   decimal:',i6)
  go to 999

900 write(*,1900)
1900 format('Invalid input')

999 end program grid4_to_g15
