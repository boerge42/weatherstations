/* *********************************************************************
 * 
 *    I2C Tcl-Extention
 *   ===================
 *    Uwe Berger; 2012
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
#ifndef __LIBIIC_H__
#define __LIBIIC_H__



#define MAX_I2C_BUF_LEN	20
 
 
/* Forwards */
int Iic_cmd (ClientData cdata, 
				  Tcl_Interp *interp, 
				  int objc, Tcl_Obj * CONST objv[]);
 

/* *************************************************** */
typedef struct {
	char 	*name;
	int 	index;
} i2c_cmd_tab_t;

#define I2C_READ	1
#define I2C_WRITE	2

#endif /* __LIBIIC_H__ */
