#pragma once
#include "../../Igc/igc.h"
// Frozen original CFSMission::UpdateAlliances from 39ef2c0. Only access to
// mission state and logging/transport are replaced; no production helper used.
namespace OriginalAlliances {
template<class Active>
void update(const char* current,SideID count,SideID sideID,SideID sideAlly,Active active,char* Allies)
{
	// filter bad/corrupted values
	// sideID: NA or a valid team ID
	if (sideID != NA)
		if (!((sideID>=0) && (sideID<c_cSidesMax))) return;
	// sideAlly: NA or a valid team ID
	if (sideAlly != NA)
		if (!((sideAlly>=0) && (sideAlly<c_cSidesMax))) return;

	// convert alliance groups to bitmasks
	int AlliesMasks[c_cSidesMax];
	for (SideID i = 0; i <  c_cSidesMax ; i++)
	{
		if (current[i] != NA)
		{
			AlliesMasks[i] = 1<<i;
			for (SideID j = 0; j <  c_cSidesMax ; j++)
			{
				if (current[j] == current[i])
					AlliesMasks[i] |= 1<<j;
			}
		}
		else
			AlliesMasks[i] = 0;
	}

	if (sideID != NA) // we got a valid parameter sideID
	{
		// update sideID AlliesMasks

		// if side is active and sideAlly parameter is a real team
		if (active(sideID) && (sideAlly != NA))
		{
		   // new allies = current allies + sideAlly + itself
		   AlliesMasks[sideID] = AlliesMasks[sideID]|(1<<sideAlly)|(1<<sideID);  // update this side allies
		}
		else
			AlliesMasks[sideID]= 0;
	}

	SideID NumSides = count;

	// propagate the alliances & count active sides
	int activesides = 0;
	for (SideID i = 0; i <  NumSides ; i++)
	{
		if (!active(i)) // i is inactive, clear its alliances
		{
			AlliesMasks[i] = 0;
			continue;
		}
		activesides++;

		if (sideID == NA) continue;
		if (i==sideID) continue;

		if (AlliesMasks[sideID] & (1<<i)) // team sideID says it's allied with i
		{
			char merged = AlliesMasks[sideID] | AlliesMasks[i];  // so merge their alliances
			AlliesMasks[sideID] = merged;
			AlliesMasks[i] = merged;
		}
		else
			AlliesMasks[i] = AlliesMasks[i] & ~(1<<sideID); // if not then make sure team i isnt ally with team sideid
	}

	// build the final allied groups
	for (SideID i = 0; i <  c_cSidesMax ; i++) Allies[i] = NA;

	if (activesides >=3) // need at least 3 active teams for alliances
	{
		// reset allies for solo teams  (remove them from their own allies mask)
		for (SideID i = 0; i <  NumSides ; i++)
		{
			if (AlliesMasks[i] == (1<<i)) AlliesMasks[i] = 0;
		}

		// propagate the alliance till it's stable
		while (true)
		{
			bool stop = true;
			for (SideID i = 0; i <  NumSides ; i++)
			{
				for (SideID j = 0; j <  NumSides ; j++)
				{
					int ai = AlliesMasks[i];
					int aj = AlliesMasks[j];
					if (ai & aj)                      // i & j have common allies
					{
						int merged = ai | aj;         // merge them
						if ((ai != merged) || (aj != merged))
						{
							stop = false;             // continue if something changed
							AlliesMasks[i] = merged;
							AlliesMasks[j] = merged;
						}
					}
				}
			}
			if (stop) break; // nothing changed after this pass, we stop
		}

		// create the different allied groups

		char groups[c_cAlliancesMax];
		for (int gr=0;gr<c_cAlliancesMax;gr++)  groups[gr]=0; // clear the groups

		int soloteams = 0; //count the solo teams found
		int numgroups = 0; //count the groups found

		for (SideID i = 0; i <  NumSides ; i++)
		{
			if (AlliesMasks[i] != 0) // this side has allies so find its group or build a new group
			{
				bool found_group = false;
				for (int g=0;g<c_cAlliancesMax;g++)
				{
					if (AlliesMasks[i] == groups[g])
					{
						Allies[i] = g;
						found_group = true; // we found its group
					}
				}
				if (!found_group)
				{
					// create a new group
					assert(numgroups<=c_cAlliancesMax) ; // shouldnt happen...
					groups[numgroups] = AlliesMasks[i];
					Allies[i] = numgroups;
					numgroups++;
				}
			}
			else
			{
				Allies[i] = NA; // this teams has no allies
				if (active(i)) soloteams++; // and it's a solo team if it's active
			}
		}
		// validation: if no group at all OR no solo teams and only 1 group then invalid alliances
		if ((numgroups==0) || ((soloteams==0) && (numgroups==1)))
		{
			// in that case, we clear all alliances
			for (SideID i = 0; i <  c_cSidesMax ; i++) Allies[i] = NA;
		}
	}// activesides >=3

}
}
