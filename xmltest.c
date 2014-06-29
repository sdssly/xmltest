/**
 * section: xmlReader
 * synopsis: Parse and validate an XML file with an xmlReader
 * purpose: Demonstrate the use of xmlReaderForFile() to parse an XML file
 *          validating the content in the process and activating options
 *          like entities substitution, and DTD attributes defaulting.
 *          (Note that the XMLReader functions require libxml2 version later
 *          than 2.6.)
 * usage: reader2 <valid_xml_filename>
 * test: reader2 test2.xml > reader1.tmp && diff reader1.tmp $(srcdir)/reader1.res
 * author: Daniel Veillard
 * copy: see Copyright for the status of this software.
 * 
 * gcc --static /xmltest.c -I/usr/include/libxml2  -lxml2   -lm -lz -llzma  
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <libxml/xmlreader.h>
#include "imx_adc.h"

extern char *optarg;
extern int optind, opterr, optopt;


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifdef LIBXML_READER_ENABLED

/**
 * processNode:
 * @reader: the xmlReader
 *
 * Dump information about the current node
 */ 
#define ADC_OPEN_CONNVALUE (4095)
#define ADC_DIRECT_CONNVALUE (0)
#define ADC_DIODE_CONNVALUE (1)

#define MAXCHANNEL (256)

#define COMP_R (0)
#define COMP_D (1)
#define COMP_C (2)

#define MAX_RESIST 	(10000000)

double ShortMin = 0;
double ShortMax = 0;

double ContMin = 0;
double ContMax = 0;
int GetConnection = 0;
int totalconnectnum = 0;

int GetComp = 0;
int totalcomp = 0;

int GetSplice = 0;
int totalsplice = 0;

int GetFixture = 0;
int totalfixture = 0;

int GetCont = 0;
int GetShort = 0;
unsigned int ContisUsed = 0;
unsigned int ShortisUsed = 0;

char g_fixturename[32];

struct stfixture { // id must < 999
	unsigned int id;
	char name[32];
};
// Save these test points
struct stfixture fixturelist[999] = {0};

struct stswitch {
	unsigned int pointA;
	unsigned int pointB;
	char name[32];
};

struct stswitch switchlist[999] = {};

struct stcompoment {
	unsigned int id;
	unsigned int type; // R/D
	char name[32];
	float value;
	int tolerance; 	// in percent
};
struct stcompoment complist[99] = {0};

struct stsplice {
	unsigned int id;
	char name[32];
};
struct stsplice splicelist[99] = {0};


struct stconnections {
	unsigned int pointA;
	unsigned int pointB;
	char name[32];
	unsigned int color;
};
struct stconnections connlist[999] = {0};

struct sttestconnections {
	unsigned int pointA;
	unsigned int pointB;
	float value;
};
struct sttestconnections testconnectionlists[999] = {0};

int a_domain[] = {8, 9, 10, 11, 117, 7, 6, 31, 30, 29};
				  
int b_domain[] = {98, 100, 44, 45, 89, 46, 87, 88, 5, 4};

int g_gpiofd[MAXCHANNEL];

// save these connection list table
float adcarray[MAXCHANNEL][MAXCHANNEL] = {};

float readADC0value[MAXCHANNEL][MAXCHANNEL] = {};
float readADC2value[MAXCHANNEL][MAXCHANNEL] = {};

// save these points used in the connection list
int testpointsA[MAXCHANNEL] = {-1};
int testpointsB[MAXCHANNEL] = {-1};
int allUsedpoints[MAXCHANNEL] = {-1};

static float GetResist(float adc0, float adc2) {
	float R = -1;
	float R2 = 2000;
	float R_switch = 5.75;

	if (adc2 == 0) { // open
		return MAX_RESIST;
	}

	R = (adc0*R2)/adc2 - R2 - (R_switch * 4);
	
	return R;
}

static char inSpliceList(unsigned int id) {
	int i = 0;
	for (i = 0; i < totalsplice; i++) {
		if (id == splicelist[i].id) {
			return 1;
		}
	}
	return 0;
}

static struct stcompoment* FindCompoment(unsigned int id) {
	int i = 0;
	for (i = 0; i < totalcomp; i++) {
		if (id == complist[i].id) {
			return &complist[i];
		}
	}
	return NULL;
}

static unsigned int tracePoint(unsigned int point) {
	struct stcompoment* pcompoment = NULL;
	int i, j;
	unsigned int realpoint = point;
	if (point < 999) {
		return point;
	}

	if (point < 81920) {
		if (!inSpliceList(point)) {
			printf("point fail!\n");
			return -1;
		}
		// find the splice point pair
		for (i = 0; i < totalconnectnum; i++) {
			if (connlist[i].pointA == point) {
				realpoint = connlist[i].pointB;
				break;
			} 
			if (connlist[i].pointB == point) {
				realpoint = connlist[i].pointA;
				break;
			}
		}

		if (realpoint > 999) {
			printf("splice pair fail\n");
			return -1;
		}
		return realpoint;
	}

	// compoment checking
	if (0 == point % 2) {
		pcompoment= FindCompoment(point);
	} else {
		pcompoment = FindCompoment(point - 1);
	}

	if (NULL == pcompoment) {
		printf("Find compoment fail! point=%d\n", point);
		return -1;
	}

		//printf("compoment type=%d value=%f\n", pcompoment->type, pcompoment->value);
		// find the compoment point pair
		for (i = 0; i < totalconnectnum; i++) {
			if (connlist[i].pointA == point) {
				realpoint = connlist[i].pointB;
				break;
			} 
			if (connlist[i].pointB == point) {
				realpoint = connlist[i].pointA;
				break;
			}
		}		
	return realpoint;
}


static void printAttribute(xmlTextReaderPtr reader)  
{  
    if(1 == xmlTextReaderHasAttributes(reader))  
    {  
        const xmlChar *name,*value;  
        int res=xmlTextReaderMoveToFirstAttribute(reader); 
        while(1 == res)  
        {  
            name=xmlTextReaderConstName(reader);  
            value=xmlTextReaderConstValue(reader);  
            printf("\tattr=[%s],val=[%s]\n",name,value);
			if (GetFixture == 1) {
				if (strcmp(name, "name") == 0) {
					printf("Fixture %s\n", value);
					strcpy(g_fixturename, value);
				}
			}

			if (GetCont == 1) {
				if (strcmp(name, "opts") == 0) {
					// check the value, should be eo/ek/em, or o/k/m
					printf("opts=%s\n", value);
					if (strcmp(value, "eo") == 0) {
						ContisUsed = 1;
					} else if (strcmp(value, "ek") == 0) {
						ContisUsed = 1000;
					} else if (strcmp(value, "em") == 0) {
						ContisUsed = (1000*1000);
					} else {
						printf("Not used the connect limit...\n");
						ContisUsed = 0;
					} 
				}

				if (strcmp(name, "val") == 0) {
					printf("val=%s\n", value);
					if (ContMin >= 0) {
						printf("Set the min/max using the first Cont element\n");
						ContMin = -(ContisUsed * atof(value));
						ContMax = (ContisUsed * atof(value));
					}
					printf("Cont range %f-%f\n", ContMin, ContMax);
				}				
			}			

			if (GetShort == 1) {
				if (strcmp(name, "opts") == 0) {
					// check the value, should be eo/ek/em, or o/k/m
					printf("opts=%s\n", value);
					if (strcmp(value, "eo") == 0) {
						ShortisUsed = 1;
					} else if (strcmp(value, "ek") == 0) {
						ShortisUsed = 1000;
					} else if (strcmp(value, "em") == 0) {
						ShortisUsed = (1000*1000);
					} else {
						printf("Not used the short limit...\n");
						ShortisUsed = 0;
					} 
				}

				if (strcmp(name, "val") == 0) {
					printf("val=%s\n", value);
					if (ShortMin >= 0) {
						printf("Set the min/max using the first short element\n");
						ShortMin = -(ShortisUsed * atof(value));
						ShortMax = (ShortisUsed * atof(value));
					}
					printf("Short range %f-%f\n", ShortMin, ShortMax);
				}				
			}

            res = xmlTextReaderMoveToNextAttribute(reader);  
        }  
        xmlTextReaderMoveToElement(reader);  
    }  
	GetCont = 0;
	GetShort = 0;
}

static void GetFixtures(const xmlChar *value) {
	char id1[32];
	char id2[32];
	char id3[32];
	int ret = 0; 
	char*	leftstring = NULL;
	//strtok 
	char buffer[256];
	char *token="\r\n";
	char *start = (char *) value;
	char *str = (char *)value;
	char *values = NULL;
	printf("%s [%s]\n", __FUNCTION__, str);

	leftstring = strstr(str, token);
	while (leftstring) {
		strncpy(buffer, (char *)str, leftstring - str);
		buffer[leftstring-str] = '\0';
		//printf("buffer=[%s] len=%zu\n", buffer, strlen(buffer));

		if (strlen(buffer) > 0) {
			values = buffer;
			while ((*values == ' ') || (*values == '\t')) {
				++values;
			}
			//printf("values=[%s] len=%zu\n", values, strlen(values));
			// get these  list
			ret = sscanf((char *)values, "%[^,],%[^,],%[^,]", id1, id2, id3);
			if (ret == 3) { // check the switch 
				connlist[totalconnectnum].pointA = strtoul(id1, NULL, 10);
				connlist[totalconnectnum].pointB = strtoul(id3, NULL, 10);
				sprintf(connlist[totalconnectnum].name, "%s%s", g_fixturename, id2); 
				connlist[totalconnectnum].color = 0;
				printf("connection:%d-%d name=%s color=%d\n", connlist[totalconnectnum].pointA,
					connlist[totalconnectnum].pointB, connlist[totalconnectnum].name, connlist[totalconnectnum].color);				
				totalconnectnum++;
			} else { // fixture
				ret = sscanf((char *)values, "%[^,],%[^,]", id1, id2);
				if (ret != 2) {
					printf("Get fixture list fail!\n");
					return;
				}
				//printf("ret=%d %s-%s\n", ret, id1, id2);
				fixturelist[strtoul(id1, NULL, 10)].id = strtoul(id1, NULL, 10); 
				sprintf(fixturelist[strtoul(id1, NULL, 10)].name, "%s%s", g_fixturename, id2);
				printf("fixture:%d name=%s\n", fixturelist[totalfixture].id, fixturelist[totalfixture].name);
				totalfixture++;	
			}
		}
		str = leftstring + strlen(token);
		leftstring = strstr((char *)str, token);
	}
	return;
}


// 65636,S1
static void GetSplices(const xmlChar *value) {
	char id1[32];
	char id2[32];
	int ret = 0; 
	char*	leftstring = NULL;
	//strtok 
	char buffer[256];
	char *token="\r\n";
	char *start = (char *) value;
	char *str = (char *)value;
	char *values = NULL;
	printf("%s [%s]\n", __FUNCTION__, str);

	leftstring = strstr(str, token);
	while (leftstring) {
		strncpy(buffer, (char *)str, leftstring - str);
		buffer[leftstring-str] = '\0';
		//printf("buffer=[%s] len=%zu\n", buffer, strlen(buffer));

		if (strlen(buffer) > 0) {
			values = buffer;
			while ((*values == ' ') || (*values == '\t')) {
				++values;
			}
			//printf("values=[%s] len=%zu\n", values, strlen(values));
			// get these  list
			ret = sscanf((char *)values, "%[^,],%[^,]", id1, 
				id2);
			//printf("ret=%d %s-%s\n", ret, id1, id2);
			if (ret != 2) {
				printf("Get splice list fail!\n");
				return ;
			}

			splicelist[totalsplice].id = strtoul(id1, NULL, 10);
			strcpy(splicelist[totalsplice].name, id2);			

			printf("splice:%d name=%s\n", splicelist[totalsplice].id, splicelist[totalsplice].name);
			
			totalsplice++;
		}
		str = leftstring + strlen(token);
		leftstring = strstr((char *)str, token);
	}
	
	return;
}

//0,7,TESTW1,0
//					2,81922,TESTW4,18
//					81923,7,TESTW5,14
//					2,65636,TESTW6,16
//					65636,2,TESTW7,0
static void GetConnections(const xmlChar *value) {
	char id1[32];
	char id2[32];
	char id3[32];
	char id4[32];
	int ret = 0; 
	char*	leftstring = NULL;
	//strtok 
	char buffer[256];
	char *token="\r\n";
	char *start = (char *) value;
	char *str = (char *)value;
	char *values = NULL;
	printf("%s [%s]\n", __FUNCTION__, str);

	leftstring = strstr(str, token);
	while (leftstring) {
		strncpy(buffer, (char *)str, leftstring - str);
		buffer[leftstring-str] = '\0';
		//printf("buffer=[%s] len=%zu\n", buffer, strlen(buffer));

		if (strlen(buffer) > 0) {
			values = buffer;
			while ((*values == ' ') || (*values == '\t')) {
				++values;
			}
			printf("values=[%s] len=%zu\n", values, strlen(values));
			// get these  list
			ret = sscanf((char *)values, "%[^,],%[^,],%[^,],%[^,]", id1, 
				id2, id3, id4);
			printf("ret=%d %s-%s-%s-%s\n", ret, id1, id2, id3, 
			id4);
			if (ret != 4) {
				printf("Get connection list fail!\n");
				return ;
			}

			connlist[totalconnectnum].pointA = strtoul(id1, NULL, 10);
			connlist[totalconnectnum].pointB = strtoul(id2, NULL, 10);
			strcpy(connlist[totalconnectnum].name, id3); 
			connlist[totalconnectnum].color = strtoul(id4, NULL, 10);
			

			printf("connection:%d-%d name=%s color=%d\n", connlist[totalconnectnum].pointA,
				connlist[totalconnectnum].pointB, connlist[totalconnectnum].name, connlist[totalconnectnum].color);
			
			totalconnectnum++;
		}
		str = leftstring + strlen(token);
		leftstring = strstr((char *)str, token);
	}
	
	return;
}
// 81920,d,D1,26,-1,90,0
// 81922,r,R1,10,0,10,k
//\r\n\t\t\t81920,d,D1,26,-1,90,0\r\n\t\t\t81922,r,R1,10,0,10,k\r\n\t\t"
static void GetCompoments(const xmlChar *value) {
	char id1[32];
	char id2[32];
	char id3[32];
	char id4[32];
	char id5[32];
	char id6[32];
	char id7[32];
	int ret = 0; 
	char*	leftstring = NULL;
	//strtok 
	char buffer[256];
	char *token="\r\n";
	char *start = (char *) value;
	char *str = (char *)value;
	char *values = NULL;
	printf("GetCompoment=[%s]\n", str);

	leftstring = strstr(str, token);
	while (leftstring) {
		strncpy(buffer, (char *)str, leftstring - str);
		buffer[leftstring-str] = '\0';
		//printf("buffer=[%s] len=%zu\n", buffer, strlen(buffer));

		if (strlen(buffer) > 0) {
			values = buffer;
			while ((*values == ' ') || (*values == '\t')) {
				++values;
			}
			//printf("values=[%s] len=%zu\n", values, strlen(values));
			// get these compoments list
			ret = sscanf((char *)values, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,]", id1, 
				id2, id3, id4, id5, id6, id7);
			//printf("ret=%d %s-%s-%s-%s-%s-%s-[%s]\n", ret, id1, id2, id3, id4, id5, id6, id7);
			if (ret != 7) {
				printf("Get compoment list fail!\n");
				return ;
			}

			complist[totalcomp].value = 0;
			complist[totalcomp].id = strtoul(id1, NULL, 10);
			strcpy(complist[totalcomp].name, id3);

			if (complist[totalcomp].id % 2 != 0) {
				printf("comp ID fail!\n");
			}
			
			if (0 == strcmp("r", id2)) {
				complist[totalcomp].type = COMP_R;
				complist[totalcomp].value = strtol(id4, NULL, 10)*
					powf(10, strtol(id5, NULL, 10));
				if (0 == strcmp("o", id7)) {
					complist[totalcomp].value *= 1;
				} else if (0 == strcmp("k", id7)) {
					complist[totalcomp].value *= 1000;
				}
				else if (0 == strcmp("m", id7)) {
					complist[totalcomp].value *= 1000000;
				} else if (0 == strcmp("M", id7)) {
					complist[totalcomp].value /= 1000000;
				} else {
					printf("R-value fail!\n");
					complist[totalcomp].value = 9999;
				}
				complist[totalcomp].tolerance = strtol(id6, NULL, 10); 
			} 
			else if (0 == strcmp("d", id2)) {
				complist[totalcomp].type = COMP_D;
				complist[totalcomp].tolerance = strtol(id6, NULL, 10);
			} else if (0 == strcmp("c", id2)) {
				complist[totalcomp].type = COMP_C;
			}  
			else {
				printf("Unknown compoment!\n");
			}

			printf("comp:type=%d id=%d name=%s value=%f tolerance=%d\n", complist[totalcomp].type, 
				complist[totalcomp].id, complist[totalcomp].name, 
				complist[totalcomp].value, complist[totalcomp].tolerance);
			
			totalcomp++;
		}
		str = leftstring + strlen(token);
		//if ((value - start) >=1) {
		//	break;
		//}
		leftstring = strstr((char *)str, token);
	}
	//leftstring = strtok(value, "\r");
	//printf("left=[%s]\n", leftstring);
	//ret = sscanf((char *)value, "%s,%s,%s,%s,%s,%s,%s", id1, 
	//	id2, id3, id4, id5, id6, id7);
	//printf("ret=%d %s-%s-%s-%s-%s-%s-%s\n", ret, id1, id2, id3, 
	//	id4, id5, id6, id7);
	//while (EOF != ret) {
	//	ret = sscanf((char *)value, "%s,%s,%s,%s,%s,%s,%s", id1, id2, id3, id4, id5, id6, id7);
	//}
	
	return;
}

static void printNode(xmlTextReaderPtr reader)  
{  int nodetype = -1;
    const xmlChar *name,*value;  
    name=xmlTextReaderConstName(reader);

	nodetype = xmlTextReaderNodeType(reader);

	//printf("PrintNode %d [%s]...\n", nodetype, name);  	
    if(nodetype ==XML_READER_TYPE_ELEMENT)
    {  
        printf("start=[%s]\n",name);

		// Get the first Cont/Short elements
		if (0 == strcmp("Cont", name)) {
			printf("start get Cont ...\n");
			GetCont = 1;
		}

		if (0 == strcmp("Short", name)) {
			printf("start get Short ...\n");
			GetShort = 1;
		}

		if (0 == strcmp("Fixture", name)) {
			printf("start get Fixture ...\n");
			g_fixturename[0] = '\0';
			GetFixture = 1;
		}

		if (0 == strcmp("Splices", name)) {
			printf("start get Splices ...\n");
			GetSplice = 1;
		}

		if (0 == strcmp("Components", name)) {
			printf("start get compoments ...\n");
			GetComp = 1;
		} 

		if (0 == strcmp("GroupInfo", name)) {
			printf("start get connections ...\n");
			GetConnection= 1;
		}
		
        printAttribute(reader);
    }
	else if(nodetype == XML_READER_TYPE_END_ELEMENT)  
    {  
    	//printf("End XML element\n");
    	g_fixturename[0] = '\0';
    	GetFixture = 0;
    	GetSplice = 0;
    	GetConnection = 0;
    	GetComp = 0;
		GetCont = 0;
		GetShort = 0;
        //printf("end=[%s]\n",name);
    } else if (nodetype == XML_READER_TYPE_COMMENT) {
		printf("comment=[%s]\n",name);
	} else if (nodetype == XML_READER_TYPE_ATTRIBUTE ) {
		printf("attribute=[%s]\n",name);
	} else if (nodetype == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {
		//printf("whitespace=[%s]\n",name);
	} else if (nodetype == XML_READER_TYPE_TEXT) {
		//printf("text=[%s]\n",name);
	} else if (nodetype == XML_READER_TYPE_CDATA) {
		//printf("CDATA=[%s]\n",name);
	}
	else
    {  
       printf("name=[%s] nodetype=%d\n",name, nodetype);  
    }

	if (nodetype == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) {}
	else {
	    if(xmlTextReaderHasValue(reader))  
	    {  
	        value=xmlTextReaderConstValue(reader);
	        printf("\tvalue=[%s]\n",value);
			if (1 == GetComp) {
				GetCompoments(value);
			}

			if (1 == GetSplice) {
				GetSplices(value);
			}

			if (1 == GetConnection) {
				GetConnections(value);
			}

			if (1 == GetFixture) {
				GetFixtures(value);
			}
			
	    }
	}
} 

static void
processNode(xmlTextReaderPtr reader) {
    const xmlChar *name, *value;

    name = xmlTextReaderConstName(reader);
    if (name == NULL)
		name = BAD_CAST "--";

    value = xmlTextReaderConstValue(reader);

    printf("%d %d %s %d %d", 
	    xmlTextReaderDepth(reader),
	    xmlTextReaderNodeType(reader),
	    name,
	    xmlTextReaderIsEmptyElement(reader),
	    xmlTextReaderHasValue(reader));

	if (value == NULL)
		printf("\n");
    else {
        //if (xmlStrlen(value) > 40)
        //    printf(" %.40s...\n", value);
        //else
	    printf("[%s\n]", value);
    }
}

/**
 * streamFile:
 * @filename: the file name to parse
 *
 * Parse, validate and print information about an XML file.
 */
static void
streamFile(const char *filename) {
    xmlTextReaderPtr reader;
    int ret;


    /*
     * Pass some special parsing options to activate DTD attribute defaulting,
     * entities substitution and DTD validation
     */
    reader = xmlReaderForFile(filename, NULL,
                 XML_PARSE_DTDATTR |  /* default DTD attributes */
		 XML_PARSE_NOENT); /* validate with the DTD */
    if (reader != NULL) {
        ret = xmlTextReaderRead(reader);
        while (ret == 1) {
            //processNode(reader);
            printNode(reader);
            ret = xmlTextReaderRead(reader);
        }
	/*
	 * Once the document has been fully parsed check the validation results
	 */
	if (xmlTextReaderIsValid(reader) != 1) {
	    fprintf(stderr, "Document %s does not validate\n", filename);
	}
        xmlFreeTextReader(reader);
        if (ret != 0) {
            fprintf(stderr, "%s : failed to parse\n", filename);
        }
    } else {
        fprintf(stderr, "Unable to open %s\n", filename);
    }
}

static void usage() {
	printf("check ADC values \n");
	return;
}

void Unexport(int gpionum)
{
  FILE *fd ;
  int pin = gpionum;

  if ((fd = fopen ("/sys/class/gpio/unexport", "w")) == NULL)
  {
    fprintf (stderr, "Unable to open GPIO export interface\n") ;
    exit (1) ;
  }

  fprintf (fd, "%d\n", pin) ;
  fclose (fd) ;
}

void UnexportALL()
{ 
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(a_domain); i++) {
		Unexport(a_domain[i]);
	}

	for (i = 0; i < ARRAY_SIZE(b_domain); i++) {
		Unexport(b_domain[i]);
	}
}

// set gpio to LOW or HIGH
void WritePin(int gpionum, char value)
{
	int pin = gpionum;
	char *mode ;
	char fName[128];

	if (g_gpiofd[pin] == -1)
	{
	  fprintf (stderr, "Unable to open GPIO direction interface for pin %d: %s\n", pin, strerror (errno)) ;
	  exit (1) ;
	}

	if (value == 0) {
		write(g_gpiofd[pin], "0\n", 2);
	} else {
		write(g_gpiofd[pin], "1\n", 2);
	}
}

// export GPIO, and set to LOW
void ExportOut0(int gpionum)
{
  FILE *fd ;
  int pin = gpionum;
  char *mode ;
  char fName [128] ;

  if ((fd = fopen ("/sys/class/gpio/export", "w")) == NULL)
  {
    fprintf (stderr, "Unable to open GPIO export interface: %s\n", strerror (errno)) ;
    exit (1) ;
  }

  fprintf(fd, "%d\n", pin) ;
  fclose(fd) ;

  sprintf(fName, "/sys/class/gpio/gpio%d/direction", pin) ;
  if ((fd = fopen (fName, "w")) == NULL)
  {
    fprintf (stderr, "Unable to open GPIO direction interface for pin %d: %s\n", pin, strerror (errno)) ;
    exit (1) ;
  }

  fprintf (fd, "out\n") ;
  fclose (fd);

// open the gpio/value sysfs
  sprintf(fName, "/sys/class/gpio/gpio%d/value", pin);
  g_gpiofd[pin] = open(fName, O_RDWR);
	  
  if (g_gpiofd[pin] == -1) {
	  fprintf (stderr, "Unable to open GPIO direction interface for pin %d: %s\n", pin, strerror (errno)) ;
	  exit (1) ;
  }
  
  WritePin(pin, 0);
}

void ExportALLOut0()
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(a_domain); i++) {
		ExportOut0(a_domain[i]);
	}

	for (i = 0; i < ARRAY_SIZE(b_domain); i++) {
		ExportOut0(b_domain[i]);
	}
}

// Doamain select one from 0-63
int writeDomain(unsigned int num, int *domain) {
	unsigned int temp = num;
	int i;
    //printf("writeDomain %d\n", num);
	for (i = 0; i < ARRAY_SIZE(a_domain); i++) {
		WritePin(domain[i], temp & 1);
		temp = temp >> 1;
	}
}

int OpenADC() {
	int adc_fd = open(IMX_ADC_DEVICE, 0);
	if (!adc_fd) {
	  printf("Error opening %s:  %s\n", IMX_ADC_DEVICE, strerror(errno));
	  exit(-1);
	} else {
	  //printf("Success!\n");
	}

	//printf("initializing the ADC%d...\n", channel);

	int err = ioctl(adc_fd, IMX_ADC_INIT);
	if (err) {
	  printf("Failure.	%d.\n", err);
	  exit(-1);
	} else {
	  //printf("Success!\n");
	}
	return adc_fd;
}

CloseADC(int adc_fd) {
	 int err = ioctl(adc_fd, IMX_ADC_DEINIT);
	  if (err) {
		printf("Failure.  %d.\n", err);
		exit(-1);
	  } else {
		//printf("Success!\n");
	  }

	err = close(adc_fd);
	if (err) {
	  printf("Error closing %s, fd was %d\n", IMX_ADC_DEVICE, adc_fd);
	  exit(-1);
	}
	adc_fd = -1;
}

float ReadADC(int adc_fd, int channel) {
	int i, j;
	int loops = 1;
	int results_per_loop = 4;
	float sum = 0;
	int err;

	//printf("starting conversion...\n");
	for (j = 0; j < loops; j++) {
	  struct t_adc_convert_param convert_param;
	  convert_param.channel=GER_PURPOSE_ADC0 + channel;
	  for (i = 0; i < 16; i++) {
		convert_param.result[i] = 0xdead;
	  }
	  err = ioctl(adc_fd, IMX_ADC_CONVERT, &convert_param);
	  if (err) {
		printf("Failure.  %d.\n", err);
	  } else {
		// printf("Success!\n");
		sum = 0;
		for (i = 0; i < results_per_loop; i++) {
		  //printf("%05f ", ((float)convert_param.result[i]));
		  sum += convert_param.result[i];
		}
		//printf("\n");
		//printf("AVG=%05f\n", sum / results_per_loop);
	  }
	  //usleep(100*2);
	}

	return (sum / results_per_loop);
}

// 256x256 20 seconds
float ReadADCAll(int adc_fd) {
	int i, j;
	int loops = 1;
	int results_per_loop = 4;
	float sum = 0;
	int err;

	//printf("starting conversion...\n");
	for (j = 0; j < loops; j++) {
	  struct t_adc_convert_param convert_param;
	  convert_param.channel=GER_PURPOSE_ADC0;
	  for (i = 0; i < 16; i++) {
		convert_param.result[i] = 0xdead;
	  }
	  err = ioctl(adc_fd, IMX_ADC_CONVERT_MULTICHANNEL, &convert_param);
	  if (err) {
		printf("Failure.  %d.\n", err);
	  } else {
		// printf("Success!\n");
		// ADC0 - ADC2
		printf("%d-%d-%d\n", convert_param.result[0], convert_param.result[1], convert_param.result[2]);
		sum = convert_param.result[0] - convert_param.result[2];
	  }
	  //usleep(100*2);
	}

	return (sum);
}


int SelfTest()
{
	int i, j;
	float adc0 = 0;
	float adc2 = 0;
	int adc_fd = -1;
	float resist = 0;

	ExportALLOut0();

	adc_fd = OpenADC();

#if 1
	for(i = 0; i < MAXCHANNEL; i++) {
		writeDomain(i, a_domain);	
		for (j = i; j < MAXCHANNEL; j++) {
			writeDomain(j, b_domain);
			//usleep(100*2);
			#if 1
			//27S
			adc0 = ReadADC(adc_fd, 0);
			adc2 = ReadADC(adc_fd, 2);
			resist = GetResist(adc0, adc2);
			printf("AB[%02d-%02d] ADC2=%f R=%f\n", i, j, adc2, resist);
			if (i == j) {// ADC2 != 0
				if (0 == adc2) {
					printf("Selfttest %d-%d fail %f\n", i, j, adc2);
				}
			} else { // ADC2 == 0
				if (0 != adc2) {
					printf("Selfttest %d-%d fail %f\n", i, j, adc2);
				}
			}
			#else
			// 20S
			sum0 = ReadADCAll(adc_fd);
			printf("AB[%02d-%02d] ADC0-ADC2=%f\n", testpointsA[i], testpointsB[i], sum0);
			#endif
		}
	}
#else
	for (i = 0; i < ARRAY_SIZE(testpointsA); i++) {
		writeDomain(testpointsA[i], a_domain);
		writeDomain(testpointsB[i], b_domain);
		sum0 = ReadADCAll(adc_fd);
		printf("AB[%02d-%02d] ADC0-ADC2=%f\n", testpointsA[i], testpointsB[i], sum0);
	}
#endif
	UnexportALL();
	CloseADC(adc_fd);

	for (i = 0; i < MAXCHANNEL; i++) {
		if (g_gpiofd[i] != -1) {
			printf("Close %d\n", i);
			close(g_gpiofd[i]);
			g_gpiofd[i] = -1;
		}
	}
	return 0;
}


int main(int argc, char **argv) {
	float resist = 0;
	float sum0 = 0;
	float sum1 = 0;
	int adc_fd = -1;
	int i = 0;
	int j = 0;
	int c = 0;
	unsigned int pointA, pointB, pointNext;
	unsigned int pointLeft, pointRight, pointPair;
	struct stcompoment* pcompoment = NULL;

	printf("ADC test build %s-%s\n", __DATE__, __TIME__);

#if 0
	while ((c = getopt(argc, argv, "ht")) > 0) {
				   switch (c) {
				   case 'h':
						   usage();
						   break;
				   default:
						   usage();
						   break;
				   }
	}


	printf("%d\n", optind);
	c = argc - optind;
	 if (c > 4 || c < 1)
			 usage ();
#endif
	
    if (argc != 2) {
        printf("a.out selftest\n");
		printf("a.out NXfile.nxf check configfile\n");
		return -1;
    }

	for (i = 0; i < MAXCHANNEL; i++) {
		g_gpiofd[i] = -1;
	}

	for (i = 0; i < MAXCHANNEL; i++) {
		for (j = 0; j < MAXCHANNEL; j++) {
			adcarray[i][j] = -1;
			readADC0value[i][j] = -1;
			readADC2value[i][j] = -1;
		}
	}

	for (i = 0; i < MAXCHANNEL; i++) {
		testpointsA[i] = -1;
		testpointsB[i] = -1;
		allUsedpoints[i] = -1;
	}

	for (i = 0; i < 999; i++) {
		fixturelist[i].name[0] = '\0';
		fixturelist[i].id = -1;
	}

	if (strcmp(argv[1], "selftest") == 0) {
		printf("perform selftest...\n");
		SelfTest();
		return 0;
	}

    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

	if (argc != 2) {
		streamFile("3.nxf");
	} else {
    	streamFile(argv[1]);
	}
    /*
     * Cleanup function for the XML library.
     */
    xmlCleanupParser();
    /*
     * this is to debug memory for regression tests
     */
    xmlMemoryDump();

	printf("\nfixture list total=%d:\n", totalfixture);
	for (i = 0; i < 999; i++) {
		if (fixturelist[i].id != -1)
		{
			printf("name=%s point=%d\n", fixturelist[i].name, fixturelist[i].id);
		}
	}


	printf("\nsplice list:\n");
	for (i = 0; i < totalsplice; i++) {
		printf("%d %s\n", splicelist[i].id, splicelist[i].name);
	}

	printf("comp list:\n");
	for (i = 0; i < totalcomp; i++) {
		printf("%d: type=%d id=%d name=%s ", i, complist[i].type, 
			complist[i].id, complist[i].name);
			if (complist[i].type == COMP_R) {
				printf("%f", complist[i].value); 	
			}
			printf("\n");
	}

	pointA = 0;
	pointB = 0;
	// TODO Display the open items
	printf("\nconnection list:\n");
	for (i = 0; i < totalconnectnum; i++) {
		printf("%d<->%d \t\tname=%s color=%d\n", connlist[i].pointA,
			connlist[i].pointB, connlist[i].name, connlist[i].color);
#if 1
		pointA = (connlist[i].pointA);
		pointB = (connlist[i].pointB);

		if ((pointA >= 65636) && (pointB >= 65636)) {
			printf("connection check fail! indirect point[%d-%d]\n", pointA, pointB);
			return -1;
		}
		
		// for splice find all these points to conencted to the same splice		
		if ((pointA < 81920) && (pointA >= 65636) && (pointB < 999)) {
			// pointA is splice
			if (!inSpliceList(pointA)) {
				printf("point fail! %d\n", pointA);
				return -1;
			}

			pointNext = -1;
			for (j = 0; j < totalconnectnum; j++) {
				if (connlist[j].pointB == pointA) {
					pointNext = connlist[j].pointA;
					//printf("nextpoint=%d\n", pointNext);
					if (pointNext < 999) {
						if (adcarray[pointB][pointNext] == ADC_DIRECT_CONNVALUE) {
							//printf("Skip dup %d-%d\n", pointNext, pointB);
						} else { adcarray[pointNext][pointB] = ADC_DIRECT_CONNVALUE;}
					} else {printf("splice warning %d...\n", pointA);}
					//break;
				} else if ((connlist[j].pointA == pointA) && (connlist[j].pointB != pointB)) {
					pointNext = connlist[j].pointB;
					if (pointNext < 999) {
						if (adcarray[pointNext][pointB] == ADC_DIRECT_CONNVALUE) {
							//printf("Skip dup %d-%d\n", pointNext, pointB);
						} else adcarray[pointB][pointNext] = ADC_DIRECT_CONNVALUE;
					} else {printf("splice warning %d...\n", pointA);}
				}  
			}
		}

		if ((pointB < 81920) && (pointB >= 65636) && (pointA < 999)) {
			// pointB is splice
			if (!inSpliceList(pointB)) {
				printf("point fail!\n");
				return -1;
			}
			pointNext = -1;
			for (j = 0; j < totalconnectnum; j++) {
				if (connlist[j].pointA == pointB) {
					pointNext = connlist[j].pointB;
					//printf("nextpoint=%d\n", pointNext);
					if (pointNext < 999) {
						if (adcarray[pointNext][pointA] == ADC_DIRECT_CONNVALUE) {
							//printf("Skip dup %d-%d\n", pointNext, pointB);
						} else adcarray[pointA][pointNext] = ADC_DIRECT_CONNVALUE;
					} else {printf("splice warning ...\n");}
					//break;
				} else if ((connlist[j].pointB == pointB) && (connlist[j].pointA != pointA)) {
					pointNext = connlist[j].pointA;
					if (pointNext < 999) {
						if (adcarray[pointNext][pointA] == ADC_DIRECT_CONNVALUE) {
							//printf("Skip dup %d-%d\n", pointNext, pointB);
						} else adcarray[pointA][pointNext] = ADC_DIRECT_CONNVALUE;
					} else {printf("splice warning ...\n");}
				}
			}
		}

		if ((pointA < 999) && (pointB < 999)) {
			printf("Direct %d-%d %f\n", pointA, pointB, adcarray[pointB][pointA]);
			if (adcarray[pointB][pointA] == ADC_DIRECT_CONNVALUE) {
				//printf("Skip dup %d-%d\n", pointNext, pointB);
			} else { 
				adcarray[pointA][pointB] = ADC_DIRECT_CONNVALUE;
			}
		}

		if ((pointA < 999) && (pointB >= 81920)) {
			//printf("Checking B...\n");
			// pointB is a compoment, should be input pin=even
			pointRight = pointLeft = -1;
			pcompoment = NULL;
			if ((pointB % 2) != 0) {
				pointRight = pointB;
				pointLeft = pointB - 1;
				//printf("warning pointB connected to outpin %d\n", pointB);
				pcompoment = FindCompoment(pointB - 1);
			} else {
				pointLeft = pointB;
				pointRight = pointB + 1;
				pcompoment= FindCompoment(pointB);
			}
	
			if (NULL == pcompoment) {
				printf("Find compoment fail! pointB=%d\n", pointB);
				return -1;
			}

			//printf("Find compoment %d\n", pcompoment->id);
			
			if (pcompoment->type == COMP_D) {// pointB is connected to a diode
				// find all these nodes directly connected to the same
				for (j = 0; j < totalconnectnum; j++) { 
					if (connlist[j].pointA == pointB) {
						pointNext = connlist[j].pointB;
						//printf("nextpoint=%d\n", pointNext);
						if (pointNext < 999) {
							if (adcarray[pointNext][pointA] == ADC_DIRECT_CONNVALUE) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointA][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("splice warning ...\n");}
						//break;
					} else if ((connlist[j].pointB == pointB) && (connlist[j].pointA != pointA)) {
						pointNext = connlist[j].pointA;
						if (pointNext < 999) {
							if (adcarray[pointNext][pointA] == ADC_DIRECT_CONNVALUE) {
							//printf("Skip dup %d-%d\n", pointNext, pointB);
						} else adcarray[pointA][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("splice warning ...\n");}
					}
				}

				// find the other node of the diode
				if ((pointB % 2) == 0) {// diode input pin
					pointPair = pointB + 1;
					for (j = 0; j < totalconnectnum; j++) {
						if (connlist[j].pointA == pointPair) {
							pointNext = connlist[j].pointB;
							if (pointNext < 999) {		
								adcarray[pointA][pointNext] = ADC_DIODE_CONNVALUE;
								adcarray[pointNext][pointA] = ADC_OPEN_CONNVALUE;
							} else {printf("splice warning ...\n");}		
						} else if ((connlist[j].pointB == pointPair) && (connlist[j].pointA != pointA)) {
							pointNext = connlist[j].pointA;
							if (pointNext < 999) {
								adcarray[pointA][pointNext] = ADC_DIODE_CONNVALUE;
								adcarray[pointNext][pointA] = ADC_OPEN_CONNVALUE;
							} else {printf("splice warning ...\n");}
						}
					}					
				} else {// diode output
					pointPair = pointB - 1;
					for (j = 0; j < totalconnectnum; j++) {
						if (connlist[j].pointA == pointPair) {
							pointNext = connlist[j].pointB;
							if (pointNext < 999) {		
								adcarray[pointA][pointNext] = ADC_OPEN_CONNVALUE;
								adcarray[pointNext][pointA] = ADC_DIODE_CONNVALUE;
							} else {printf("splice warning ...\n");}		
						} else if ((connlist[j].pointB == pointPair) && (connlist[j].pointA != pointA)) {
							pointNext = connlist[j].pointA;
							if (pointNext < 999) {
								adcarray[pointA][pointNext] = ADC_OPEN_CONNVALUE;
								adcarray[pointNext][pointA] = ADC_DIODE_CONNVALUE;
							} else {printf("splice warning ...\n");}
						}
					}
				}
			}
			else if (pcompoment->type == COMP_R) {// pointB is connected to resistor
				pointNext = -1;
				// find all these nodes directly connected to the resistor
				for (j = 0; j < totalconnectnum; j++) { 
					if (connlist[j].pointA == pointB) {
						pointNext = connlist[j].pointB;
						//printf("nextpoint=%d\n", pointNext);
						if (pointNext < 999) {
							if (adcarray[pointNext][pointA] == ADC_DIRECT_CONNVALUE) {
							//printf("Skip dup %d-%d\n", pointNext, pointB);
						} else adcarray[pointA][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("splice warning ...\n");}
						//break;
					} else if ((connlist[j].pointB == pointB) && (connlist[j].pointA != pointA)) {
						pointNext = connlist[j].pointA;
						if (pointNext < 999) {
							if (adcarray[pointNext][pointA] == ADC_DIRECT_CONNVALUE) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else 	adcarray[pointA][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("splice warning ...\n");}
					}
				}

				// find the other node of the resistor
				if ((pointB % 2) == 0) {
					pointPair = pointB + 1;
				} else {
					pointPair = pointB - 1;
				}

				pointNext = -1;
				for (j = 0; j < totalconnectnum; j++) {
					if (connlist[j].pointA == pointPair) {
						pointNext = connlist[j].pointB;
						if (pointNext < 999) {
							if (adcarray[pointNext][pointA] == pcompoment->value) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointA][pointNext] = pcompoment->value;
						} else {printf("splice warning 2...\n");}		
					} else if ((connlist[j].pointB == pointPair) && (connlist[j].pointA != pointA)) {
						pointNext = connlist[j].pointA;
						if (pointNext < 999) {
							if (adcarray[pointNext][pointA] == pcompoment->value) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointA][pointNext] = pcompoment->value;
						} else {printf("splice warning 3...\n");}
					}
				}
			} else {
				printf("connection unsupport fail!\n");	
			}
			
			if (pointNext < 999) {
			} else {
				printf("Resistor1 fail %d\n", pointNext);
			}
		}

		if ((pointB < 999) && (pointA >= 81920)) {
			//printf("Checking A...\n");
			// pointA is a compoment, should be input pin=even
			pointRight = pointLeft = -1;
			pcompoment = NULL;
			if ((pointA % 2) != 0) {
				pointRight = pointA;
				pointLeft = pointA - 1;
				//printf("warning pointA connected to outpin %d\n", pointA);
				pcompoment = FindCompoment(pointA - 1);
			} else {
				pointLeft = pointA;
				pointRight = pointA + 1;
				pcompoment= FindCompoment(pointA);
			}
	
			if (NULL == pcompoment) {
				printf("Find compoment fail! pointA=%d\n", pointA);
				return -1;
			}

			//printf("find compoment id=%d\n", pcompoment->id);
			
			if (pcompoment->type == COMP_D) {// pointA=diode pin
				// find all these nodes directly connected to the same
				for (j = 0; j < totalconnectnum; j++) {
					if (connlist[j].pointB == pointA) {
						pointNext = connlist[j].pointA;
						//printf("nextpoint=%d\n", pointNext);
						if (pointNext < 999) {
							if (adcarray[pointNext][pointB] == ADC_DIRECT_CONNVALUE) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointB][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("splice warning D1...\n");}
						//break;
					} else if ((connlist[j].pointA == pointA) && (connlist[j].pointB != pointB)) {
						pointNext = connlist[j].pointB;
						if (pointNext < 999) {
							if (adcarray[pointNext][pointB] == ADC_DIRECT_CONNVALUE) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointB][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("splice warning D2...\n");}
					}
				}

				// find the other node of the diode
				if ((pointA % 2) == 0) {// pontA=diode input pin
					//printf("PointA is diode in...\n");
					pointPair = pointA + 1;
					for (j = 0; j < totalconnectnum; j++) {
						if (connlist[j].pointB == pointPair) {
							pointNext = connlist[j].pointA;
							//printf("Din=%d\n", pointNext);
							if (pointNext < 999) {		
								adcarray[pointB][pointNext] = ADC_DIODE_CONNVALUE;
								adcarray[pointNext][pointB] = ADC_OPEN_CONNVALUE;
							} else {printf("splice warning D3...\n");}		
						} else if ((connlist[j].pointA == pointPair) && (connlist[j].pointB != pointB)) {
							pointNext = connlist[j].pointB;
							if (pointNext < 999) {
								adcarray[pointB][pointNext] = ADC_DIODE_CONNVALUE;
								adcarray[pointNext][pointB] = ADC_OPEN_CONNVALUE;
							} else {printf("splice warning D4...\n");}
						}
					}					
				} else {// diode output
					pointPair = pointA - 1; // PointA=Diode output
					for (j = 0; j < totalconnectnum; j++) {
						if (connlist[j].pointB == pointPair) {
							pointNext = connlist[j].pointA;
							//printf("Dout=%d\n", pointNext);
							if (pointNext < 999) {		
								adcarray[pointB][pointNext] = ADC_OPEN_CONNVALUE;
								adcarray[pointNext][pointB] = ADC_DIODE_CONNVALUE;
							} else {printf("splice warning D5...\n");}		
						} else if ((connlist[j].pointA == pointPair) && (connlist[j].pointB != pointB)) {
							pointNext = connlist[j].pointB;
							if (pointNext < 999) {
								adcarray[pointB][pointNext] = ADC_OPEN_CONNVALUE;
								adcarray[pointNext][pointB] = ADC_DIODE_CONNVALUE;
							} else {printf("splice warning D6...\n");}
						}
					}
				}
			}
			else if (pcompoment->type == COMP_R) {// pointA is a resistor pin
				pointNext = -1;
				// find all these nodes directly connected to the resistor
				for (j = 0; j < totalconnectnum; j++) {
					if (connlist[j].pointB == pointA) {
						pointNext = connlist[j].pointA;
						//printf("nextpoint=%d\n", pointNext);
						if (pointNext < 999) {
							if (pointNext == pointB) {
								printf("R warning loop ... \n");
							}
							if (adcarray[pointNext][pointB] == ADC_DIRECT_CONNVALUE) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointB][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("R warning1 ...\n");}
					} else if ((connlist[j].pointA == pointA) && (connlist[j].pointB != pointB)) {
						pointNext = connlist[j].pointB;
						if (pointNext < 999) {
							if (adcarray[pointNext][pointB] == ADC_DIRECT_CONNVALUE) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointB][pointNext] = ADC_DIRECT_CONNVALUE;
						} else {printf("R warning2 ...\n");}
					}
				}

				// find the other node of the resistor
				if ((pointA % 2) == 0) {
					pointPair = pointA + 1;
				} else {
					pointPair = pointA - 1;
				}

				pointNext = -1;
				for (j = 0; j < totalconnectnum; j++) {
					if (connlist[j].pointA == pointPair) {
						pointNext = connlist[j].pointB;
						if (pointNext < 999) {
							if (pointNext == pointB) { printf("resistor warning loop ...\n");}
							if (adcarray[pointNext][pointB] == pcompoment->value) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointB][pointNext] = pcompoment->value;
						} else {printf("splice warning R5...\n");}		
					} else if ((connlist[j].pointB == pointPair)) {
						pointNext = connlist[j].pointA;
						if (pointNext < 999) {
							if (pointNext == pointB) { printf("resistor warning loop ...\n");}
							if (adcarray[pointNext][pointB] == pcompoment->value) {
								//printf("Skip dup %d-%d\n", pointNext, pointB);
							} else adcarray[pointB][pointNext] = pcompoment->value;
						} else {printf("splice warning R6...\n");}
					}
				}
			} else {
				printf("connection unsupport fail!\n");	
			}
		}	
	#endif	
		// 81920 = first compoment ; 65636= first splice	
	}

	printf("\nADC check table:\n");
	for (i = 0; i < MAXCHANNEL; i++) {
		for (j = 0; j < MAXCHANNEL; j++) {
			if (-1 != adcarray[i][j]) {
				printf("adcarray[%d-%d]=%fohm\n", i, j, adcarray[i][j]);
				testpointsA[i] = 1;
				testpointsB[j] = 1;
			}
		}
	}

	printf("\nTest point A list:\n");
	for (i = 0; i < MAXCHANNEL; i++) {
		if (testpointsA[i] != -1) {
			printf("%d[name=%s] ", i, fixturelist[i].name);
		}
	}

	printf("\nTest point B list:\n");
	for (i = 0; i < MAXCHANNEL; i++) {
		if (testpointsB[i] != -1) {
			printf("%d[name=%s] ", i, fixturelist[i].name);
		}
	}	
	
	printf("\n");
#if 1
	printf("\nStart ADC...\n");

	ExportALLOut0();
	adc_fd = OpenADC();
	
   	// check all these points in testpointA/B
	for (i = 0; i < MAXCHANNEL; i++) {	
		if (testpointsA[i] == -1) {// skip unused points in group A
	   		continue;
		}
		allUsedpoints[i] = 1;
	   	writeDomain(i, a_domain);
	   	for (j = 0; j < MAXCHANNEL; j++) {
	   		if (i == j) {continue; } // skip self-test
			if (testpointsB[j] == -1) { // skip unused points in group B 
				continue;
			}
			allUsedpoints[j] = 1;
			
			if ((adcarray[i][j] == ADC_DIODE_CONNVALUE)
				|| (adcarray[i][j] == ADC_OPEN_CONNVALUE)) {// need to rad adc[ij] and adc[ji]
				printf("Diode adcarray[%d-%d]=%f...\n", i, j, adcarray[i][j]);
			} else if ((adcarray[i][j] == ADC_DIRECT_CONNVALUE)
				|| (adcarray[i][j] == -1) ||
				((adcarray[i][j] > 0))) {//direct/undef/resist
					printf("adcarray[%d-%d]=%f\n", i, j, adcarray[i][j]);
					if (-1 != readADC0value[j][i]) {
						printf("resist/short/undef skip %d-%d read=%f\n", i, j, readADC0value[j][i]);
						readADC0value[i][j] = readADC0value[j][i];
						readADC2value[i][j] = readADC2value[j][i];
					} else { //
						printf("No ADC value!\n");
					}
			}  
			else { // FIXME check the undef connections(adcarray=4095)
				printf("ADC error! adcarray[%d-%d]=%f\n", i, j, adcarray[i][j]);
			} 

			if (readADC0value[i][j] == -1) { // no adc value
				writeDomain(j, b_domain);
				sum0 = ReadADC(adc_fd, 0);
				sum1 = ReadADC(adc_fd, 2);
				readADC0value[i][j] = sum0;
				readADC2value[i][j] = sum1;
				printf("Read %d-%d ADC0=%f ADC2=%f\n", i, j, sum0, sum1);
			}
			//compar with adcarray
			//if ( i == j)
			resist = GetResist(readADC0value[i][j], readADC2value[i][j]);
			printf("AB[%d-%d] adcarray=%f ADC0=%f ADC2=%f R=%f\n", 
				i, j, adcarray[i][j], readADC0value[i][j], readADC2value[i][j], resist);
			
			if (adcarray[i][j] == ADC_OPEN_CONNVALUE) {
				if (resist == MAX_RESIST) {
					printf("***open %d-%d PASS!\n", i, j);
				} else {
					printf("***open %d-%d FAIL! %f\n", i, j, resist);
				}
			} else if (adcarray[i][j] == ADC_DIODE_CONNVALUE) {
					if ((resist > 0) && (resist < MAX_RESIST)) {
						printf("***DIOD %d-%d PASS %f\n", i, j, resist);
					} else {
						printf("***DIOD %d-%d FAIL %f\n", i, j, resist);
					}
				} 
			else if (adcarray[i][j] == ADC_DIRECT_CONNVALUE) {
					if ((resist > ContMin) && (resist < ContMax)) {
						printf("***directconnection %d-%d PASS %f\n", i, j, resist);
					} else {
						printf("***directconnection %d-%d FAIL %f\n", i, j, resist);
					}
				}
			else if (adcarray[i][j] == -1) {
				if (resist == MAX_RESIST) {
					printf("***disconnect %d-%d PASS %f\n", i, j, resist);
				} else {
					printf("***disconnect %d-%d FAIL %f\n", i, j, resist);
				}
			} else { // resist
				if ((adcarray[i][j] > 0) && (adcarray[i][j] < 100)) {
					if ((resist > (adcarray[i][j] - 5)) && (resist < (adcarray[i][j] + 5))) {
						printf("***resist %d-%d PASS %f==%f\n", i, j, adcarray[i][j], resist);
					} else {
						printf("***resist %d-%d FAIL %f==%f\n", i, j, adcarray[i][j], resist);
					}
				} else if ((adcarray[i][j] >= 100) && (adcarray[i][j] < 10000)) {
					if ((resist > (adcarray[i][j] * 0.95)) && (resist < (adcarray[i][j] * 1.05))) {
						printf("***resist %d-%d PASS %f==%f\n", i, j, adcarray[i][j], resist);
					} else {
						printf("***resist %d-%d FAIL %f==%f\n", i, j, adcarray[i][j], resist);
					}				
				} else if ((adcarray[i][j] >= 10000) && (adcarray[i][j] < 50000)) {
					if ((resist > (adcarray[i][j] * 0.9)) && (resist < (adcarray[i][j] * 1.1))) {
						printf("***resist %d-%d PASS %f==%f\n", i, j, adcarray[i][j], resist);
					} else {
						printf("***resist %d-%d FAIL %f==%f\n", i, j, adcarray[i][j], resist);
					}				
				} else {
					printf("***resist %d-%d FAIL %f==%f\n", i, j, adcarray[i][j], resist);
				} 
			}
	   }
   }

// TODO if all connection test PASS, display the test result, and replace the cables; 
// TODO when all connection are open, then start a new tests
// TODO step2 test all connections in the used-pin list
	printf("Test all used points:\n");
	for (i = 0; i < MAXCHANNEL; i++) {
		if (allUsedpoints[i] != -1) {
			printf("%d\n", i);
		}
	}
	
	for (i = 0; i < MAXCHANNEL; i++) {
		if (allUsedpoints[i] == -1) {
			continue;
		};
		for (j = i; j < MAXCHANNEL; j++) {
			if (allUsedpoints[j] == -1) {
				continue;
			};
			printf("Test %d-%d\n", i, j);
		}
	}
   UnexportALL();
   CloseADC(adc_fd);
#endif
    return(0);
}

#else
#endif

