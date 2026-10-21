#include <cassert>
#include <climits>
#include <iostream>
#include "Match.h"
#include "HeroCard.h"
#include "DMCard.h"
template<class F> void rejects(F f) { bool threw=false; try { f(); } catch(const std::exception&) { threw=true; } assert(threw); }
int main() {
 Hero healer("h","Healer",{HeroClass::HEALER,30},3);
 healer.takeDamage(-5); assert(healer.getHealth()==30);
 healer.takeDamage(INT_MAX); assert(!healer.isAlive()); healer.heal(INT_MAX); assert(healer.getHealth()==30);
 Trap trap(5); trap.trigger(healer); trap.trigger(healer); assert(healer.getHealth()==25);
 assert(healer.drawCard()==nullptr);
 Hero warrior("w","Warrior",{HeroClass::WARRIOR,30},3);
 rejects([&]{warrior.addToDeck(std::make_unique<HeroCard>("Heal",1,HeroEffect::HEAL,5));});
 rejects([&]{healer.addToDeck(std::make_unique<DMCard>("Summon",1,DMEffect::SUMMON,1));});
 HeroCard heal("Heal",1,HeroEffect::HEAL,5); heal.applyEffect(healer); assert(healer.getHealth()==30);
 Match match("test");
 auto h=std::make_unique<Hero>("hero","Hero",HeroType{HeroClass::WARRIOR,30},10); auto* hv=h.get();
 auto d=std::make_unique<DungeonMaster>("dm","DM",3); auto* dv=d.get();
 auto m=std::make_unique<Monster>("Goblin",20); auto* mv=m.get();
 for(int i=0;i<6;++i) h->addToDeck(std::make_unique<HeroCard>("Strike",1,HeroEffect::ATTACK,6));
 d->addToDeck(std::make_unique<DMCard>("Summon",1,DMEffect::SUMMON,1));
 match.addPlayer(std::move(h)); match.addPlayer(std::move(d)); match.addMonster(std::move(m)); match.start();
 rejects([&]{match.endTurn(*dv);});
 HeroCard fake("Fake",0,HeroEffect::ATTACK,100);
 assert(!match.validate(*hv,fake,*mv).valid);
 rejects([&]{match.apply(fake,*mv);}); assert(mv->getHealth()==20);
 assert(!match.validate(*hv,hv->getHandCard(0),*hv).valid);
 hv->drawCard(); hv->drawCard(); hv->drawCard();
 for(int i=0;i<3;++i) match.apply(hv->getHandCard(0),*mv);
 assert(mv->getHealth()==2 && hv->getMana()==7 && hv->getDiscardSize()==3);
 assert(!match.validate(*hv,hv->getHandCard(0),*mv).valid);
 match.endTurn(*hv); assert(match.getActivePlayer()==dv);
 assert(!match.validate(*dv,dv->getHandCard(0),*mv).valid);
 rejects([&]{match.apply(dv->getHandCard(0),*mv);}); assert(dv->getMana()==3);
 match.endTurn(*dv); assert(match.getRound()==2 && hv->getMana()==10);
 match.apply(hv->getHandCard(0),*mv); assert(match.getWinner()==Side::HEROES);
 rejects([&]{match.endTurn(*hv);});
 Match multi("multi",1); Hero* heroes[3];
 for(int i=0;i<3;++i) {
  auto x=std::make_unique<Hero>(std::to_string(i),"Hero",HeroType{HeroClass::RANGER,30},3);
  heroes[i]=x.get(); multi.addPlayer(std::move(x));
 }
 auto dm=std::make_unique<DungeonMaster>("dm","DM",3); auto* dmView=dm.get();
 multi.addPlayer(std::move(dm)); multi.addMonster(std::make_unique<Monster>("M",20)); multi.start();
 for(auto* v:heroes) { assert(multi.getActivePlayer()==v); multi.endTurn(*v); }
 assert(multi.getActivePlayer()==dmView); multi.endTurn(*dmView);
 assert(multi.getWinner()==Side::DUNGEON_MASTER);
 Match expensive("costly");
 auto e=std::make_unique<Hero>("e","E",HeroType{HeroClass::WARRIOR,30},0); auto* ev=e.get();
 e->addToDeck(std::make_unique<HeroCard>("Costly",1,HeroEffect::ATTACK,1));
 auto enemy=std::make_unique<Monster>("M",20); auto* enemyView=enemy.get();
 expensive.addPlayer(std::move(e)); expensive.addPlayer(std::make_unique<DungeonMaster>("d","D",3));
 expensive.addMonster(std::move(enemy)); expensive.start();
 assert(!expensive.validate(*ev,ev->getHandCard(0),*enemyView).valid);
 std::cout << "All model checks passed.\n";
}
