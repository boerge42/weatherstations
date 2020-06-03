/* *********************************************************************
 * 
 *    I2C Tcl-Extention
 *   ===================
 *    Uwe Berger; 2012
 * 
 * 
 * Tcl-Syntax:
 * -----------
 * i2c read  <device> <i2c-addr> <b|w>
 * i2c write <device> <i2c-addr> <buffer>
 * 
 * --> i2c read --> return a integer
 * 
 * 
 * ---------
 * Have fun!
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *     
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * 
 * *********************************************************************
 */

#include <tcl.h>
#include <string.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include "libiic.h"
 

i2c_cmd_tab_t i2c_cmd_tab[] = {
	{"read",	I2C_READ	},
	{"write",	I2C_WRITE	},
	{0, 0}
};

/* *************************************************** */
int Iic_search_cmd (char *name) {
	int i;
	for (i = 0; i2c_cmd_tab[i].index != 0; i++) {
		if (strncmp(name, i2c_cmd_tab[i].name , strlen(name)) == 0 ) 
			return i2c_cmd_tab[i].index ;
	};
	return 0;
};

/* *************************************************** */
int i2c_write(char *device, int adress, int *buffer, int len) {
	int  file;
	int i;
	unsigned char buf[MAX_I2C_BUF_LEN];
	for (i=0; i<len; i++) buf[i]=buffer[i];
	file = open(device, O_RDWR);
	if (file < 0) return 1;
	if (ioctl(file, I2C_SLAVE, adress)) return 1;
	if (write(file, &buf, len) != len) return 1;	
	close(file);
	return 0;
}

/* *************************************************** */
int i2c_read(char *device, int adress, int reg, int len, int* ret) {
	int  file;
	int i;
	unsigned char buf[MAX_I2C_BUF_LEN];
	file = open(device, O_RDWR);
	if (file < 0) return 1;
	if (ioctl(file, I2C_SLAVE, adress)) return 1;
	// welches Register gelesen werden soll, steht in reg...
	// ...und das muss erstmal gesendet werden
	buf[0]=reg;
	if (write(file, &buf, 1) != 1) return 1;
	// Antwort lesen, Anzahl Bytes wird vorgegeben
	if (read(file, buf, len) != len)  return 1;
	close(file);
	(*ret)=buf[0];
	// ...wenn Word, dann zusammenrechnen...
	if (len==2) (*ret)=(buf[1]<<8)+buf[0];
	return 0;
}


/* *************************************************** */
/* *************************************************** */
/* *************************************************** */
/* Erweiterung initialisieren */
int Iic_Init (Tcl_Interp *interp) {

#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, "8.1", 0) == 0L) {
		return TCL_ERROR;
	}
#endif

	/* i2c-Kommando erzeugen */
	Tcl_CreateObjCommand (interp, "i2c", Iic_cmd, NULL, NULL);
	return TCL_OK;
}

/* *************************************************** */
/* *************************************************** */
/* *************************************************** */
/* i2c-Kommando */
int Iic_cmd (ClientData cdata,
				  Tcl_Interp *interp, 
				  int objc,
				  Tcl_Obj * CONST objv[]) {
					  
	int index, i;
	int adr;
	char *dev, *mode;
	Tcl_Obj** buf;
	int buf_len;
	int i2c_buffer[MAX_I2C_BUF_LEN];
	int ret, reg;
					  	
	// mindestens i2c <befehl>
	if (objc < 2) {
		Tcl_WrongNumArgs (interp, 1, objv, "?command?");
		return TCL_ERROR;
	}
	
	// Unterkommando suchen/verzweigen
	index = Iic_search_cmd(Tcl_GetString (objv[1]));
	if (index==0) {
		Tcl_WrongNumArgs (interp, 1, objv, "read|write");
		return TCL_ERROR ;
	};
	switch (index) {
		case I2C_READ:
			// Parameterpruefung
			if (objc !=6) {
				Tcl_WrongNumArgs (interp, 1, objv, "read ?device? ?addr? ?reg? ?mode?");
				return TCL_ERROR;
			}
			// Parameter parsen (soweit notwendig...)
			// ...I2C-Adresse
			if (Tcl_GetIntFromObj(interp, objv[3], &adr) != TCL_OK) {
				Tcl_Obj *retval = Tcl_NewStringObj("i2c read: addr must be a number", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;							
			}
			// ...Register
			if (Tcl_GetIntFromObj(interp, objv[4], &reg) != TCL_OK) {
				Tcl_Obj *retval = Tcl_NewStringObj("i2c read: reg must be a number", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;							
			}
			// ...Mode (Byte oder Word)
			mode=Tcl_GetString (objv[5]);
			if (strncmp(mode, "b" , strlen(mode)) == 0 ) {
				buf_len = 1;
			} else if (strncmp(mode, "w" , strlen(mode)) == 0 ) {
				buf_len = 2;
			} else {
				Tcl_Obj *retval = Tcl_NewStringObj("i2c read: mode must be \"b\" or \"w\"", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;											
			}
			// Befehl ausfuehren
			if (i2c_read(Tcl_GetString(objv[2]), adr, reg, buf_len, &ret)) {
				Tcl_Obj *retval = Tcl_NewStringObj("Error i2c_read()", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;			
			}
			// Return
			Tcl_Obj* retval = Tcl_NewIntObj(ret);
			Tcl_SetObjResult (interp, retval);
			break;
			
		case I2C_WRITE:
			// Parameterpruefung
			if (objc !=5) {
				Tcl_WrongNumArgs (interp, 1, objv, "read ?device? ?addr? ?buf?");
				return TCL_ERROR;
			}
			// Parameter parsen (soweit notwendig...)
			// ...I2C-Adresse
			if (Tcl_GetIntFromObj(interp, objv[3], &adr) != TCL_OK) {
				Tcl_Obj *retval = Tcl_NewStringObj("i2c write: addr must be a number", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;							
			}
			// ...zu schreibende Bytes als im 4.Argument als Liste
			if (Tcl_ListObjGetElements(interp, objv[4], &buf_len, &buf) != TCL_OK) {
				Tcl_Obj *retval = Tcl_NewStringObj("i2c write: buf must be a list", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;							
			}
			// ...max. Pufferlaenge pruefen
			if (buf_len>MAX_I2C_BUF_LEN) {
				Tcl_Obj *retval = Tcl_NewStringObj("i2c: too many values in buf", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;											
			}
			// ...Werte aus Liste extrahieren
			for (i=0; i<buf_len; i++) {
				if (Tcl_GetIntFromObj(interp, buf[i], &i2c_buffer[i]) != TCL_OK) {
					Tcl_Obj *retval = Tcl_NewStringObj("i2c write: element of buf must be a number", -1);
					Tcl_SetObjResult (interp, retval);
					return TCL_ERROR;							
				}
			}
			// Befehl ausfuehren
			if (i2c_write(Tcl_GetString(objv[2]), adr, i2c_buffer, buf_len)) {
				Tcl_Obj *retval = Tcl_NewStringObj("Error i2c_write()", -1);
				Tcl_SetObjResult (interp, retval);
				return TCL_ERROR;			
			}
			
			break;
			
	}

	return TCL_OK;
}
