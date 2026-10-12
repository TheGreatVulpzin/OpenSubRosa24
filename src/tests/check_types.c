/* Compila os headers gerados. Os static_assert conferem tamanhos e offsets. */
#include "sr_types.h"
#include "sr_globals_024d.h"

int main(void)
{
    return (int)(sizeof(Player) + sizeof(Human) + sizeof(Item) + sizeof(Object) + sizeof(Connection)
                 + sizeof(ItemType) + sizeof(ObjectType) + sizeof(Bond) + sizeof(Event) + sizeof(Bullet)
                 + sizeof(Body) + sizeof(Account) + sizeof(Game)) == 0;
}
