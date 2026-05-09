-- Insert one temporary tsql CRUD test row.
insert into questions (fingerprint,title,answer,type,confidence,course,confirm_count) values ('__TEST_FP__','tsql CRUD temporary test question','A','0','low','tsql-test',1);

select id,fingerprint,title,answer,type,confidence,course,confirm_count
from questions
where fingerprint = '__TEST_FP__'
limit 1;
