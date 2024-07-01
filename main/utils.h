
unsigned int crc16(unsigned int crc, unsigned char *buf, int len)
{
    for (int pos = 0; pos < len; pos++)
    {
        crc ^= (unsigned int)buf[pos];

        for (int i = 8; i != 0; i--)
        {
            if ((crc & 0x0001) != 0)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}


uint16_t crc16_full (const uint8_t *data, unsigned int length)
{
    // Polynomial: x^16 + x^15 + x^2 + 1 (0xa001)
	
    uint16_t crc = 0;

    while (length--) {
    	
		int i;
	
		crc ^= *data++;
		for (i = 0 ; i < 8 ; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xa001;
			else
				crc = (crc >> 1);
		}    			
    }
    
    return crc;
}

bool isNumber(char *res, int len)
{
    for (int i = 0; i < len; i++)
    {
        if (((res[i] < '0') || (res[i] > '9')) && (res[i] != '.' && res[i] != 0))
        {
            return false;
        }
    }
    return true;
}

int findCharInArrayRev(char array[], char c, int len)
{
    for (int i = len - 1; i >= 0; i--)
    {
        if (array[i] == c)
        {
            return i;
        }
    }

    return -1;
}
