============================
 Ceph s3 select Feature List
============================
Features Support
----------------

The following table describes the support for s3-select functionalities:

+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Feature                         | Detailed        | Example                                                               |
+=================================+=================+=======================================================================+
| Arithmetic operators            | ^ * / + - ( )   | select (int(_1)+int(_2))*int(_9) from stdin;                          |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
|                                 |                 | select ((1+2)*3.14) ^ 2 from stdin;                                   |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Compare operators               | > < >= <= == != | select _1,_2 from stdin where (int(1)+int(_3))>int(_5);               |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| logical operator                | AND OR          | select count(*) from stdin where int(1)>123 and int(_5)<200;          |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| casting operator                | int(expression) | select int(_1),int( 1.2 + 3.4) from stdin;                            |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
|                                 |float(expression)|                                                                       |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
|                                 | timestamp(...)  | select timestamp("1999:10:10-12:23:44") from stdin;                   |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Aggregation Function            | sum             | select sum(int(_1)) from stdin;                                       |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Aggregation Function            | min             | select min( int(_1) * int(_5) ) from stdin;                           |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Aggregation Function            | max             | select max(float(_1)),min(int(_5)) from stdin;                        |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Aggregation Function            | count           | select count(*) from stdin where (int(1)+int(_3))>int(_5);            |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Timestamp Functions             | extract         | select count(*) from stdin where                                      |
|                                 |                 |       extract("year",timestamp(_2)) > 1950                            |    
|                                 |                 |            and extract("year",timestamp(_1)) < 1960;                  |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Timestamp Functions             | dateadd         | select count(0) from stdin where                                      |
|                                 |                 | datediff("year",timestamp(_1),dateadd("day",366,timestamp(_1))) == 1; |  
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Timestamp Functions             | datediff        | select count(0) from stdin where                                      |  
|                                 |                 | datediff("month",timestamp(_1),timestamp(_2))) == 2;                  | 
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| Timestamp Functions             | utcnow          | select count(0) from  stdin where                                     |
|                                 |                 |    datediff("hours",utcnow(),dateadd("day",1,utcnow())) == 24 ;       |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
| String Functions                | substr          |  select count(0) from  stdin where                                    |
|                                 |                 |    int(substr(_1,1,4))>1950 and int(substr(_1,1,4))<1960;             |
+---------------------------------+-----------------+-----------------------------------------------------------------------+
|                                 |                 |                                                                       |
+---------------------------------+-----------------+-----------------------------------------------------------------------+

Sending Query to RGW
====================


Syntax
~~~~~~
CSV default defintion for field-delimiter,row-delimiter,quote-char,escape-char are: { , \\n " \\ }

::

 aws --endpoint-url http://localhost:8000 s3api select-object-content 
  --bucket {BUCKET-NAME}  
  --expression-type 'SQL'     
  --input-serialization 
  '{"CSV": {"FieldDelimiter": "," , "QuoteCharacter": "\"" , "RecordDelimiter" : "\n" , "QuoteEscapeCharacter" : "\\" }, "CompressionType": "NONE"}' 
  --output-serialization '{"CSV": {}}' 
  --key {OBJECT-NAME} 
  --expression "select count(0) from stdin where int(_1)<10;" output.csv



