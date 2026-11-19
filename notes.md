В таске везде упоминается, что данные получаются и отдаются в форме массива --
это конкретное указание или "общее слово";
данные можно хранить в любом контейнере?
(Решение: приняла буквально, массив)

В тз сказано, что массив размера 0 -- это валидные входные данные?
(Решение: пусть будут валидные, однако сообщим пользователю о его странном
 решении)

there are padding bytes in the StatData struct, which should be
zero'd to keep all nice and neat, but since there is no other consumers
except LoadDump, which simply ignores the values of padding bytes, I left
them as they are
