program free_text_to_f71

  character*13 c13,w
  character*71 f71
  character*42 c
  character*1 qa(10),qb(10)
  data c/' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?'/

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Usage:   free_text_to_f71 "<message>"'
     print*,'Example: free_text_to_f71 "TNX BOB 73 GL"'
     go to 999
  endif
  call getarg(1,c13)
  call mp_short_init
  qa=char(0)
  w=adjustr(c13)
  do i=1,13
     j=index(c,w(i:i))-1
     if(j.lt.0) j=0
     call mp_short_mult(qb,qa(2:10),9,42)    !qb(1:9)=42*qa(2:9)
     call mp_short_add(qa,qb(2:10),9,j)      !qa(1:9)=qb(2:9)+j
  enddo
  write(f71,1000) qa(2:10)
1000 format(b7.7,8b8.8)
  write(*,1010) c13,f71
1010 format('Free text: ',a13/'f71: ',a71)

999 end program free_text_to_f71

subroutine mp_short_ops(w,u)
! Multi-precision arithmetic with storage in character arrays.  
  character*1 w(*),u(*)
  integer i,ireg,j,n,ir,iv,ii1,ii2
  character*1 creg(4)
  save ii1,ii2
  equivalence (ireg,creg)

  entry mp_short_init
  ireg=256*ichar('2')+ichar('1')
  do j=1,4
     if (creg(j).eq.'1') ii1=j
     if (creg(j).eq.'2') ii2=j
  enddo
  return

  entry mp_short_add(w,u,n,iv)
  ireg=256*iv
  do j=n,1,-1
     ireg=ichar(u(j))+ichar(creg(ii2))
     w(j+1)=creg(ii1)
  enddo
  w(1)=creg(ii2)
  return

  entry mp_short_mult(w,u,n,iv)
  ireg=0
  do j=n,1,-1
     ireg=ichar(u(j))*iv+ichar(creg(ii2))
     w(j+1)=creg(ii1)
  enddo
  w(1)=creg(ii2)
  return

  return
end subroutine mp_short_ops
