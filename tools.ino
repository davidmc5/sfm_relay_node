//////////////////////////////////////////
//////////////////////////////////////////
// THESE USE String OBJECTS
//CONVERT TO REGULAR STRING ARRAYS.
//////////////////////////////////////////
//////////////////////////////////////////

/** Is this an IP? */
boolean isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

void removeChar( char* str, char t ){
  /* https://stackoverflow.com/a/7822057 */
  int i,j;
  i = 0;
  while(i<strlen(str))
  {
      if (str[i]==t) 
      { 
          for (j=i; j<strlen(str); j++)
              str[j]=str[j+1];   
      } else i++;
  }
}
