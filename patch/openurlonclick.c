void
openUrlOnClick(int col, int row, char* url_opener)
{
   int row_start = row;
   int col_start = col;
   int row_end = row;
   int col_end = col;

   if (term.line[row][col].u == ' ')
       return;

   /* while previous character is not space */
   while (term.line[row_start][col_start-1].u != ' ') {
       if (col_start == 0)
       {
           // Before moving start pointer to the previous line we check if it ends with space
           if (term.line[row_start - 1][term.col - 1].u == ' ')
               break;
           col_start=term.col - 1;
           row_start--;
       } else {
           col_start--;
       }
   }

   /* while next character is not space nor end of line */
   while (term.line[row_end][col_end].u != ' ') {
       col_end++;
       if (col_end == term.col - 1)
       {
           if (term.line[row_end + 1][0].u == ' ')
               break;
           col_end=0;
           row_end++;
       }
   }

   char url[200] = "";
   int url_index=0;
   do {
       url[url_index] = term.line[row_start][col_start].u;
       url_index++;
       col_start++;
       if (col_start == term.col)
       {
           col_start = 0;
           row_start++;
       }
   } while (url_index < (sizeof(url)-1) &&
            (row_start != row_end || col_start != col_end));

   if (strncmp("http", url, 4) != 0) {
       return;
   }

   char command[strlen(url_opener)+strlen(url)+2];
   sprintf(command, "%s %s", url_opener, url);
   system(command);
}
