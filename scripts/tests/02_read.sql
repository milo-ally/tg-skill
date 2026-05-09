-- Read the temporary tsql CRUD test row.
select id,fingerprint,title,answer,type,confidence,course,confirm_count
from questions
where fingerprint = '__TEST_FP__'
limit 1;
