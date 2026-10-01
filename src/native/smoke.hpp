#pragma once
namespace Native {
class Game;
int diagnosticShipCount(Game& game);
void maintainScaleObserver(Game& game);
void arrangeScaleBattle(Game& game,bool spread=false,bool idle=false,float observerDistance=1500,bool commandObserver=false);
// Diagnostic setup uses the same IGC authority callbacks as the local server.
void prepareEliminationSmoke(Game& game);
void loseEliminationSmokeStations(Game& game);
int prepareCrewUiSmoke(Game& game);
void completeResultsUiSmoke(Game& game);
}
