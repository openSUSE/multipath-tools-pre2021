 /*
 *****************************************************************************
 *                                                                           *
 *     (C)  Copyright 2007 Hewlett-Packard Development Company, L.P          *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify it   *
 * under the terms of the GNU General Public License as published by the Free*
 * Software  Foundation; either version 2 of the License, or (at your option)*
 * any later version.                                                        *
 *                                                                           *
 * This program is distributed in the hope that it will be useful, but       *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY*
 * or FITNESS FOR  A PARTICULAR PURPOSE. See the GNU General Public License  *
 * for more details.                                                         *
 *                                                                           *
 * You should have received a copy of the GNU General Public License along   *
 * with this program; if not, write to the Free Software Foundation, Inc.,   *
 * 675 Mass Ave, Cambridge, MA 02139, USA.                                   *
 *                                                                           *
 * The copy of the GNU General Public License is available at                *
 * /opt/hp/HPDMmultipath-tool directoy                                       *
 *                                                                           *
 *****************************************************************************
 */

/*
 * Prioritizer for device mapper,when a path instance is provided as the
 * preferred path.

 * This prioritizer assigns a priority value based on the comparison made
 * between the preferred path and the path instance for which this is called.
 * A priority value of 1 is assigned to the preferred path and 0 to the other
 * non-preferred paths.

 * Returns zero on successful assignment of priority and -1 on failure.
 * Failure to assign priority can be caused due to invalid pathname or a missing * argument.
 */

#include<stdio.h>
#include<string.h>
#define HIGH 1
#define LOW 0
#define FILE_NAME_SIZE 256

int main(int argc, char * argv[])
{

	char path[FILE_NAME_SIZE];
	
	if(argv[1] && argv[2])
	{
		if(!strncmp(argv[2],"/dev/",5))
			strcpy(path,argv[2]+5);

		if(!strcmp(path,argv[1]) || !strcmp(argv[1],argv[2]))
		{
			printf("%u\n", HIGH);
			return 0;
		}
		else
		{
			printf("%u\n", LOW);
			return 0;
		}
	}
	return -1;
}
