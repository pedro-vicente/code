C----------------------------------------------------------------------------
C NeXus - Neutron & X-ray Common Data Format
C  
C API Fortran Interface
C  
C $Id$
C
C Copyright (C) 1997, Freddie Akeroyd
C                     ISIS Facility, Rutherford Appleton Laboratory, UK
C  
C See NAPI.C for further details
C
C 22/2/98 - Correct an NXfclose problem (free() of FORTRAN memory)
C 97/7/30 - Initial Release
C 97/7/31 - Correct NXPUTATTR/NXGETATTR and make 'implicit none' clean
C 97/8/7  - Update interface
C----------------------------------------------------------------------------

C *** Return length of a string, ignoring training blanks
      INTEGER FUNCTION TRUELEN(STRING)
      CHARACTER*(*) STRING
      DO TRUELEN=LEN(STRING),1,-1
          IF (STRING(TRUELEN:TRUELEN) .NE. ' ') RETURN
      ENDDO
      TRUELEN = 0
      END

C *** Convert FORTRAN string STRING into NULL terminated C string ISTRING
      SUBROUTINE EXTRACT_STRING(ISTRING, LENMAX, STRING)
      CHARACTER*(*) STRING
      INTEGER I,ILEN,TRUELEN,LENMAX
      BYTE ISTRING(LENMAX)
      EXTERNAL TRUELEN
      ILEN = TRUELEN(STRING)
      IF (ILEN .GE. LENMAX) THEN
          WRITE(6,9000) LENMAX, ILEN+1
          RETURN
      ENDIF
      DO I=1,ILEN
          ISTRING(I) = ICHAR(STRING(I:I))
      ENDDO
      ISTRING(ILEN+1) = 0
      RETURN
 9000 FORMAT('NAPIF: String too long - buffer needs increasing from ',
     +        i4,' to at least ',i4)
      END

C *** Convert NULL terminated C string ISTRING to FORTRAN string STRING
      SUBROUTINE REPLACE_STRING(STRING, ISTRING)
      BYTE ISTRING(*)
      CHARACTER*(*) STRING
      INTEGER I
      STRING = ' '
      DO I=1,LEN(STRING)
          IF (ISTRING(I) .EQ. 0) RETURN
          STRING(I:I) = CHAR(ISTRING(I))
      ENDDO
      WRITE(6,9010) LEN(STRING) 
      RETURN
 9010 FORMAT('NAPIF: String truncated - buffer needs to be > ', I4)
      END

C *** Wrapper routines for NXAPI interface
      INTEGER FUNCTION NXOPEN(FILENAME, ACCESS_METHOD, FILEID)
      CHARACTER*(*) FILENAME
      BYTE IFILENAME(256)
      INTEGER ACCESS_METHOD
      INTEGER FILEID(*),NXIFOPEN
      EXTERNAL NXIFOPEN
      CALL EXTRACT_STRING(IFILENAME, 256, FILENAME)
      NXOPEN = NXIFOPEN(IFILENAME, ACCESS_METHOD, FILEID)
      END

      INTEGER FUNCTION NXCLOSE(FILEID)
      INTEGER FILEID(*),NXIFCLOSE
      EXTERNAL NXIFCLOSE
      NXCLOSE = NXIFCLOSE(FILEID)
      END

      INTEGER FUNCTION NXMAKEGROUP(FILEID, VGROUP, NXCLASS)
      INTEGER FILEID(*),NXIMAKEGROUP
      CHARACTER*(*) VGROUP, NXCLASS
      BYTE IVGROUP(256), INXCLASS(256)
      EXTERNAL NXIMAKEGROUP
      CALL EXTRACT_STRING(IVGROUP, 256, VGROUP)
      CALL EXTRACT_STRING(INXCLASS, 256, NXCLASS)
      NXMAKEGROUP = NXIMAKEGROUP(FILEID, IVGROUP, INXCLASS)
      END

      INTEGER FUNCTION NXOPENGROUP(FILEID, VGROUP, NXCLASS)
      INTEGER FILEID(*),NXIOPENGROUP
      CHARACTER*(*) VGROUP, NXCLASS
      BYTE IVGROUP(256), INXCLASS(256)
      EXTERNAL NXIOPENGROUP
      CALL EXTRACT_STRING(IVGROUP, 256, VGROUP)
      CALL EXTRACT_STRING(INXCLASS, 256, NXCLASS)
      NXOPENGROUP = NXIOPENGROUP(FILEID, IVGROUP, INXCLASS)
      END

      INTEGER FUNCTION NXCLOSEGROUP(FILEID)
      INTEGER FILEID(*),NXICLOSEGROUP
      EXTERNAL NXICLOSEGROUP
      NXCLOSEGROUP = NXICLOSEGROUP(FILEID)
      END

      INTEGER FUNCTION NXMAKEDATA(FILEID, LABEL, DATATYPE, RANK, DIM)  
      INTEGER FILEID(*), DATATYPE, RANK, DIM(*), NXIFMAKEDATA
      CHARACTER*(*) LABEL
      BYTE ILABEL(256)
      EXTERNAL NXIFMAKEDATA
      CALL EXTRACT_STRING(ILABEL, 256, LABEL)
      NXMAKEDATA = NXIFMAKEDATA(FILEID, ILABEL, DATATYPE, RANK, DIM) 
      END

      INTEGER FUNCTION NXOPENDATA(FILEID, LABEL)
      INTEGER FILEID(*),NXIOPENDATA
      CHARACTER*(*) LABEL
      BYTE ILABEL(256)
      EXTERNAL NXIOPENDATA
      CALL EXTRACT_STRING(ILABEL, 256, LABEL)
      NXOPENDATA = NXIOPENDATA(FILEID, ILABEL)
      END

      INTEGER FUNCTION NXCLOSEDATA(FILEID)
      INTEGER FILEID(*),NXICLOSEDATA
      EXTERNAL NXICLOSEDATA
      NXCLOSEDATA = NXICLOSEDATA(FILEID)
      END

      INTEGER FUNCTION NXGETDATA(FILEID, DATA)
      INTEGER FILEID(*), DATA(*),NXIGETDATA
      EXTERNAL NXIGETDATA
      NXGETDATA = NXIGETDATA(FILEID, DATA)
      END

      INTEGER FUNCTION NXGETSLAB(FILEID, DATA, START, SIZE)
      INTEGER FILEID(*), DATA(*), START(*), SIZE(*)
      INTEGER NXIGETSLAB
      EXTERNAL NXIGETSLAB
      NXGETSLAB = NXIGETSLAB(FILEID, DATA, START, SIZE)
      END
      
      INTEGER FUNCTION NXIFGETCHARATTR(FILEID, INAME, DATA, 
     +                                 DATALEN, TYPE)
      INTEGER MAX_DATALEN,NX_ERROR
      INTEGER FILEID(*), DATALEN, TYPE
      PARAMETER(MAX_DATALEN=1024,NX_ERROR=0)
      CHARACTER*(*) DATA
      BYTE IDATA(MAX_DATALEN)
      BYTE INAME(*)
      INTEGER NXIGETATTR
      EXTERNAL NXIGETATTR
      IF (DATALEN .GE. MAX_DATALEN) THEN
          WRITE(6,9020) DATALEN, MAX_DATALEN
          NXIFGETCHARATTR=NX_ERROR
          RETURN
      ENDIF
      NXIFGETCHARATTR = NXIGETATTR(FILEID, INAME, IDATA, DATALEN, TYPE)
      IF (NXIFGETCHARATTR .NE. NX_ERROR) THEN
          CALL REPLACE_STRING(DATA, IDATA)
      ENDIF
      RETURN
 9020 FORMAT('NXgetattr: asked for attribute size ', I4, 
     +       ' with buffer size only ', I4)
      END

      INTEGER FUNCTION NXGETATTR(FILEID, NAME, DATA, DATALEN, TYPE)
      INTEGER FILEID(*),DATALEN,TYPE,NX_CHAR
      PARAMETER(NX_CHAR=4)
      CHARACTER*(*) NAME, DATA
      BYTE INAME(256)
      INTEGER NXIGETATTR, NXIFGETCHARATTR
      EXTERNAL NXIGETATTR, NXIFGETCHARATTR
      CALL EXTRACT_STRING(INAME, 256, NAME)
      IF (TYPE .EQ. NX_CHAR) THEN
          NXGETATTR = NXIFGETCHARATTR(FILEID, INAME, DATA,
     +                                DATALEN, TYPE)
      ELSE
          NXGETATTR = NXIGETATTR(FILEID, INAME, DATA, DATALEN, TYPE)
      ENDIF
      END

      INTEGER FUNCTION NXPUTDATA(FILEID, DATA)
      INTEGER FILEID(*), DATA(*), NXIPUTDATA
      EXTERNAL NXIPUTDATA
      NXPUTDATA = NXIPUTDATA(FILEID, DATA)
      END

      INTEGER FUNCTION NXPUTSLAB(FILEID, DATA, START, SIZE)
      INTEGER FILEID(*), DATA(*), START(*), SIZE(*)
      INTEGER NXIPUTSLAB
      EXTERNAL NXIPUTSLAB
      NXPUTSLAB = NXIPUTSLAB(FILEID, DATA, START, SIZE)
      END

      INTEGER FUNCTION NXIFPUTCHARATTR(FILEID, INAME, DATA, 
     +                                 DATALEN, TYPE)
      INTEGER FILEID(*), DATALEN, TYPE 
      BYTE INAME(*)
      CHARACTER*(*) DATA
      BYTE IDATA(1024)
      INTEGER NXIFPUTATTR
      EXTERNAL NXIFPUTATTR
      CALL EXTRACT_STRING(IDATA, 1024, DATA)
      NXIFPUTCHARATTR = NXIFPUTATTR(FILEID, INAME, IDATA, DATALEN, TYPE)
      END

      INTEGER FUNCTION NXPUTATTR(FILEID, NAME, DATA, DATALEN, TYPE)
      INTEGER FILEID(*), DATALEN, TYPE, NX_CHAR
      PARAMETER(NX_CHAR=4)
      CHARACTER*(*) NAME, DATA
      BYTE INAME(256)
      INTEGER NXIFPUTATTR, NXIFPUTCHARATTR
      EXTERNAL NXIFPUTATTR, NXIFPUTCHARATTR
      CALL EXTRACT_STRING(INAME, 256, NAME)
      IF (TYPE .EQ. NX_CHAR) THEN
          NXPUTATTR = NXIFPUTCHARATTR(FILEID, INAME, DATA, 
     +                                DATALEN, TYPE)
      ELSE
          NXPUTATTR = NXIFPUTATTR(FILEID, INAME, DATA, DATALEN, TYPE)
      ENDIF
      END

      INTEGER FUNCTION NXGETINFO(FILEID, RANK, DIM, DATATYPE)
      INTEGER FILEID(*), RANK, DIM(*), DATATYPE
      INTEGER I, J, NXIGETINFO
      EXTERNAL NXIGETINFO
      NXGETINFO = NXIGETINFO(FILEID, RANK, DIM, DATATYPE)
C *** Reverse dimension array as C is ROW major, FORTRAN column major
      DO I = 1, RANK/2
          J = DIM(I)
          DIM(I) = DIM(RANK-I+1)
          DIM(RANK-I+1) = J
      ENDDO
      END

      INTEGER FUNCTION NXGETNEXTENTRY(FILEID, NAME, CLASS, DATATYPE)
      INTEGER FILEID(*), DATATYPE
      CHARACTER*(*) NAME, CLASS
      BYTE INAME(256), ICLASS(256)
      INTEGER NXIGETNEXTENTRY
      EXTERNAL NXIGETNEXTENTRY
      NXGETNEXTENTRY = NXIGETNEXTENTRY(FILEID, INAME, ICLASS, DATATYPE)
      CALL REPLACE_STRING(NAME, INAME)
      CALL REPLACE_STRING(CLASS, ICLASS)
      END

      INTEGER FUNCTION NXGETNEXTATTR(FILEID, PNAME, ILENGTH, ITYPE)
      INTEGER FILEID(*), ILENGTH, ITYPE, NXIGETNEXTATTR
      CHARACTER*(*) PNAME
      BYTE IPNAME(1024)
      EXTERNAL NXIGETNEXTATTR
      NXGETNEXTATTR = NXIGETNEXTATTR(FILEID, IPNAME, ILENGTH, ITYPE)
      CALL REPLACE_STRING(PNAME, IPNAME)
      END

      INTEGER FUNCTION NXGETGROUPID(FILEID, LINK)
      INTEGER FILEID(*), LINK(*), NXIGETGROUPID
      EXTERNAL NXIGETGROUPID
      NXGETGROUPID = NXIGETGROUPID(FILEID, LINK)
      END

      INTEGER FUNCTION NXGETDATAID(FILEID, LINK)
      INTEGER FILEID(*), LINK(*), NXIGETDATAID
      EXTERNAL NXIGETDATAID
      NXGETDATAID = NXIGETDATAID(FILEID, LINK)
      END

      INTEGER FUNCTION NXMAKELINK(FILEID, LINK)
      INTEGER FILEID(*), LINK(*), NXIMAKELINK
      EXTERNAL NXIMAKELINK
      NXMAKELINK = NXIMAKELINK(FILEID, LINK)
      END
