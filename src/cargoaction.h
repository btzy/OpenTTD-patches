/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargoaction.h Actions to be applied to cargo packets. */

#ifndef CARGOACTION_H
#define CARGOACTION_H

#include "cargopacket.h"

/** Base class for CargoRemoval. */
class CargoRemovalAmount {
private:
	uint amount;    ///< Amount of cargo still unprocessed.

public:
	CargoRemovalAmount (uint amount) : amount (amount)
	{
	}

	/** Get the amount of cargo still unprocessed. */
	uint Amount (void) const
	{
		return this->amount;
	}

	uint Preprocess (CargoPacket *cp);
	bool Postprocess (CargoPacket *cp, uint remove);
};

/**
 * Abstract action for moving cargo from one list to another.
 * @tparam Tsource CargoList subclass to remove cargo from.
 * @tparam Tdest CargoList subclass to add cargo to.
 */
template<class Tsource, class Tdest>
class CargoMovement {
protected:
	Tsource *source;    ///< Source of the cargo.
	Tdest *destination; ///< Destination for the cargo.
	uint max_move;      ///< Maximum amount of cargo to be moved with this action.
	CargoPacket *Preprocess(CargoPacket *cp);
public:
	CargoMovement(Tsource *source, Tdest *destination, uint max_move) : source(source), destination(destination), max_move(max_move) {}

	/**
	 * Returns how much more cargo can be moved with this action.
	 * @return Amount of cargo this action can still move.
	 */
	uint MaxMove() { return this->max_move; }
};

/** Action of transferring cargo from a vehicle to a station. */
class CargoTransfer : public CargoMovement<VehicleCargoList, StationCargoList> {
public:
	CargoTransfer(VehicleCargoList *source, StationCargoList *destination, uint max_move) :
			CargoMovement<VehicleCargoList, StationCargoList>(source, destination, max_move) {}
	bool operator()(CargoPacket *cp);
};

/** Action of loading cargo from a station onto a vehicle. */
class CargoLoad : public CargoMovement<StationCargoList, VehicleCargoList> {
protected:
	TileIndex load_place; ///< TileIndex to be saved in the packets' loaded_at_xy.
public:
	CargoLoad(StationCargoList *source, VehicleCargoList *destination, uint max_move, TileIndex load_place) :
			CargoMovement<StationCargoList, VehicleCargoList>(source, destination, max_move), load_place(load_place) {}
	bool operator()(CargoPacket *cp);
};

/** Action of reserving cargo from a station to be loaded onto a vehicle. */
class CargoReservation : public CargoLoad {
public:
	CargoReservation(StationCargoList *source, VehicleCargoList *destination, uint max_move, TileIndex load_place) :
			CargoLoad(source, destination, max_move, load_place) {}
	bool operator()(CargoPacket *cp);
};

/** Action of returning previously reserved cargo from the vehicle to the station. */
class CargoReturn : public CargoMovement<VehicleCargoList, StationCargoList> {
public:
	CargoReturn(VehicleCargoList *source, StationCargoList *destination, uint max_move) :
			CargoMovement<VehicleCargoList, StationCargoList>(source, destination, max_move) {}
	bool operator()(CargoPacket *cp);
};

/** Action of shifting cargo from one vehicle to another. */
class CargoShift : public CargoMovement<VehicleCargoList, VehicleCargoList> {
public:
	CargoShift(VehicleCargoList *source, VehicleCargoList *destination, uint max_move) :
			CargoMovement<VehicleCargoList, VehicleCargoList>(source, destination, max_move) {}
	bool operator()(CargoPacket *cp);
};

/** Action of rerouting cargo between different cargo lists and/or next hops. */
template<class Tlist>
class CargoReroute : public CargoMovement<Tlist, Tlist> {
protected:
	StationID avoid;
	StationID avoid2;
	const GoodsEntry *ge;
public:
	CargoReroute (Tlist *list, StationID avoid, StationID avoid2, const GoodsEntry *ge, uint max_move = UINT_MAX) :
			CargoMovement<Tlist, Tlist> (list, list, max_move), avoid(avoid), avoid2(avoid2), ge(ge) {}
};

/** Action of rerouting cargo in a station. */
class StationCargoReroute : public CargoReroute<StationCargoList> {
public:
	StationCargoReroute (StationCargoList *list, StationID avoid, StationID avoid2, const GoodsEntry *ge) :
			CargoReroute<StationCargoList> (list, avoid, avoid2, ge) {}
	bool operator()(CargoPacket *cp);
};

/** Action of rerouting cargo staged for transfer in a vehicle. */
class VehicleCargoReroute : public CargoReroute<VehicleCargoList> {
public:
	VehicleCargoReroute (VehicleCargoList *list, uint max_move, StationID avoid, StationID avoid2, const GoodsEntry *ge) :
			CargoReroute<VehicleCargoList> (list, avoid, avoid2, ge, max_move)
	{
		assert(this->max_move <= source->ActionCount(VehicleCargoList::MTA_TRANSFER));
	}
	bool operator()(CargoPacket *cp);
};

#endif /* CARGOACTION_H */
