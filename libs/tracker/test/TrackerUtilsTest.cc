#include <set>
#include <map>
#include <string>

#include "evoral/ControlList.h"
#include "evoral/Note.h"
#include "temporal/bbt_time.h"

#include "tracker_utils.h"

#include "TrackerUtilsTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION (TrackerUtilsTest);

using namespace Tracker;

//////////////////////
// Number formatting //
//////////////////////

void
TrackerUtilsTest::testNumberPointHelpers ()
{
	CPPUNIT_ASSERT_EQUAL (std::string ("1"), TrackerUtils::rm_point_zeros ("1.0000"));
	CPPUNIT_ASSERT_EQUAL (std::string ("1.23"), TrackerUtils::rm_point_zeros ("1.2300"));
	CPPUNIT_ASSERT_EQUAL (std::string ("1"), TrackerUtils::rm_point_zeros ("1."));
	CPPUNIT_ASSERT_EQUAL (std::string ("1"), TrackerUtils::rm_point_zeros ("1"));
	CPPUNIT_ASSERT_EQUAL (std::string ("-1.5"), TrackerUtils::rm_point_zeros ("-1.50"));

	CPPUNIT_ASSERT_EQUAL (std::string ("100"), TrackerUtils::num_to_string (100, 10, 2));
	CPPUNIT_ASSERT_EQUAL (std::string ("-100"), TrackerUtils::num_to_string (-100, 10, 2));
	CPPUNIT_ASSERT_EQUAL (std::string ("FF"), TrackerUtils::num_to_string (255, 16, 2));
	CPPUNIT_ASSERT_EQUAL (std::string ("-FF"), TrackerUtils::num_to_string (-255, 16, 2));

	CPPUNIT_ASSERT_EQUAL (std::string ("100.5"), TrackerUtils::num_to_string (100.5, 10, 2));
	CPPUNIT_ASSERT_EQUAL (std::string ("1"), TrackerUtils::num_to_string (1.0, 10, 2));
	/* 255 in hex with 2 fractional digits is FF.00, unpadded to FF */
	CPPUNIT_ASSERT_EQUAL (std::string ("FF"), TrackerUtils::num_to_string (255.0, 16, 2));
}

void
TrackerUtilsTest::testPositionHelpers ()
{
	CPPUNIT_ASSERT_EQUAL ((size_t) 3, TrackerUtils::point_position ("123.0"));
	CPPUNIT_ASSERT_EQUAL ((size_t) 3, TrackerUtils::point_position ("123"));
	CPPUNIT_ASSERT_EQUAL ((size_t) 0, TrackerUtils::point_position (".5"));

	CPPUNIT_ASSERT (TrackerUtils::position_range ("123.0") == std::make_pair (-1, 2));
	CPPUNIT_ASSERT (TrackerUtils::position_range ("123") == std::make_pair (0, 2));
	CPPUNIT_ASSERT (TrackerUtils::position_range ("0123") == std::make_pair (0, 3));
}

void
TrackerUtilsTest::testPadUnpad ()
{
	CPPUNIT_ASSERT_EQUAL (std::string ("123.0"), TrackerUtils::pad ("123", -1));
	CPPUNIT_ASSERT_EQUAL (std::string ("123"), TrackerUtils::pad ("123", 0));
	CPPUNIT_ASSERT_EQUAL (std::string ("0123"), TrackerUtils::pad ("123", 3));
	CPPUNIT_ASSERT_EQUAL (std::string ("-0123"), TrackerUtils::pad ("-123", 3));
	CPPUNIT_ASSERT_EQUAL (std::string ("100.00"), TrackerUtils::pad ("100", -2));

	CPPUNIT_ASSERT_EQUAL (std::string ("0123"), TrackerUtils::non_negative_pad ("123", 3));
	CPPUNIT_ASSERT_EQUAL (std::string ("123"), TrackerUtils::non_negative_pad ("123", 0));

	CPPUNIT_ASSERT_EQUAL (std::string ("123"), TrackerUtils::int_unpad ("0123"));
	CPPUNIT_ASSERT_EQUAL (std::string ("-123"), TrackerUtils::int_unpad ("-0123"));
	CPPUNIT_ASSERT_EQUAL (std::string ("FF"), TrackerUtils::int_unpad ("00FF", 16));

	CPPUNIT_ASSERT_EQUAL (std::string ("0.0123"), TrackerUtils::insert_point ("123", -1));
	CPPUNIT_ASSERT_EQUAL (std::string ("0.123"), TrackerUtils::insert_point ("123", 0));
	CPPUNIT_ASSERT_EQUAL (std::string ("1.23"), TrackerUtils::insert_point ("123", 1));
	CPPUNIT_ASSERT_EQUAL (std::string ("12.3"), TrackerUtils::insert_point ("123", 2));
	CPPUNIT_ASSERT_EQUAL (std::string ("123"), TrackerUtils::insert_point ("123", 3));

	CPPUNIT_ASSERT_EQUAL ((size_t) 4, TrackerUtils::locate ("123.15", -1));
	CPPUNIT_ASSERT_EQUAL ((size_t) 4, TrackerUtils::locate ("123.1", -1));
	CPPUNIT_ASSERT_EQUAL ((size_t) 2, TrackerUtils::locate ("123", 0));
	CPPUNIT_ASSERT_EQUAL ((size_t) 0, TrackerUtils::locate ("0123", 3));
	CPPUNIT_ASSERT_EQUAL ((size_t) std::string::npos, TrackerUtils::locate ("0123", 4));
}

void
TrackerUtilsTest::testDigitHelpers ()
{
	CPPUNIT_ASSERT_EQUAL ('0', TrackerUtils::digit_to_char (0, 10));
	CPPUNIT_ASSERT_EQUAL ('5', TrackerUtils::digit_to_char (5, 16));
	CPPUNIT_ASSERT_EQUAL ('A', TrackerUtils::digit_to_char (10, 16));
	CPPUNIT_ASSERT_EQUAL ('F', TrackerUtils::digit_to_char (15, 16));

	CPPUNIT_ASSERT_EQUAL (0, TrackerUtils::char_to_digit ('0', 10));
	CPPUNIT_ASSERT_EQUAL (7, TrackerUtils::char_to_digit ('7', 10));
	CPPUNIT_ASSERT_EQUAL (10, TrackerUtils::char_to_digit ('A', 16));
	CPPUNIT_ASSERT_EQUAL (15, TrackerUtils::char_to_digit ('F', 16));
}

void
TrackerUtilsTest::testSignAndHexPrefix ()
{
	CPPUNIT_ASSERT (!TrackerUtils::is_negative ("1"));
	CPPUNIT_ASSERT (TrackerUtils::is_negative ("-1"));
	CPPUNIT_ASSERT (!TrackerUtils::is_negative (""));

	CPPUNIT_ASSERT (!TrackerUtils::has_hex_prefix ("A"));
	CPPUNIT_ASSERT (!TrackerUtils::has_hex_prefix ("-A"));
	CPPUNIT_ASSERT (TrackerUtils::has_hex_prefix ("0xA"));
	CPPUNIT_ASSERT (TrackerUtils::has_hex_prefix ("-0xA"));
	CPPUNIT_ASSERT (TrackerUtils::has_hex_prefix ("0XA"));

	CPPUNIT_ASSERT_EQUAL (std::string ("0xA"), TrackerUtils::append_hex_prefix ("A"));
	CPPUNIT_ASSERT_EQUAL (std::string ("-0xA"), TrackerUtils::append_hex_prefix ("-A"));
	CPPUNIT_ASSERT_EQUAL (std::string ("0xA"), TrackerUtils::append_hex_prefix ("0xA"));
	CPPUNIT_ASSERT_EQUAL (std::string ("-0xA"), TrackerUtils::append_hex_prefix ("-0xA"));
}

void
TrackerUtilsTest::testNumberValidation ()
{
	CPPUNIT_ASSERT (TrackerUtils::is_number<int> ("123"));
	CPPUNIT_ASSERT (TrackerUtils::is_number<int> ("-123"));
	CPPUNIT_ASSERT (TrackerUtils::is_number<int> ("+123"));
	CPPUNIT_ASSERT (!TrackerUtils::is_number<int> ("12a"));
	CPPUNIT_ASSERT (TrackerUtils::is_number<double> ("1.5"));
	CPPUNIT_ASSERT (TrackerUtils::is_number<double> ("-1.5"));
	CPPUNIT_ASSERT (!TrackerUtils::is_number<double> ("1..5"));
	CPPUNIT_ASSERT (TrackerUtils::is_number<int> ("FF", 16));
	CPPUNIT_ASSERT (!TrackerUtils::is_number<int> ("FF", 10));

	CPPUNIT_ASSERT_EQUAL (123, TrackerUtils::string_to_num<int> ("123"));
	CPPUNIT_ASSERT_EQUAL (-123, TrackerUtils::string_to_num<int> ("-123"));
	CPPUNIT_ASSERT_EQUAL (255, TrackerUtils::string_to_num<int> ("ff", 16));
	CPPUNIT_ASSERT_EQUAL (-255, TrackerUtils::string_to_num<int> ("-ff", 16));
	CPPUNIT_ASSERT_DOUBLES_EQUAL (1.5, TrackerUtils::string_to_num<double> ("1.5"), 1e-9);
}

void
TrackerUtilsTest::testChangeDigit ()
{
	CPPUNIT_ASSERT_DOUBLES_EQUAL (100.05, TrackerUtils::change_digit<double> (100.0, 5, -2, 10, 2), 1e-9);
	CPPUNIT_ASSERT_DOUBLES_EQUAL (100.5, TrackerUtils::change_digit<double> (100.0, 5, -1, 10, 2), 1e-9);
	CPPUNIT_ASSERT_EQUAL (105, TrackerUtils::change_digit<int> (100, 5, 0));
	CPPUNIT_ASSERT_EQUAL (150, TrackerUtils::change_digit<int> (100, 5, 1));
	CPPUNIT_ASSERT_EQUAL (500, TrackerUtils::change_digit<int> (100, 5, 2));
	CPPUNIT_ASSERT_EQUAL (5100, TrackerUtils::change_digit<int> (100, 5, 3));
}

void
TrackerUtilsTest::testChangeDigitOrSign ()
{
	CPPUNIT_ASSERT_EQUAL (105, TrackerUtils::change_digit_or_sign<int> (100, 5, 0));
	CPPUNIT_ASSERT_EQUAL (-100, TrackerUtils::change_digit_or_sign<int> (100, -1, 0));
	CPPUNIT_ASSERT_EQUAL (100, TrackerUtils::change_digit_or_sign<int> (-100, 10, 0));
	CPPUNIT_ASSERT_EQUAL (100, TrackerUtils::change_digit_or_sign<int> (100, 10, 0));
}

void
TrackerUtilsTest::testPitch ()
{
	CPPUNIT_ASSERT_EQUAL ((uint8_t) 48, TrackerUtils::pitch (0, 4));
	CPPUNIT_ASSERT_EQUAL ((uint8_t) 57, TrackerUtils::pitch (9, 4));
	CPPUNIT_ASSERT_EQUAL ((uint8_t) 119, TrackerUtils::pitch (11, 9));
}

void
TrackerUtilsTest::testParsePitch ()
{
	/* a missing octave is filled with the default one */
	CPPUNIT_ASSERT_EQUAL (TrackerUtils::parse_pitch ("C4", 4), TrackerUtils::parse_pitch ("C", 4));
	/* when the octave is present the default is irrelevant */
	CPPUNIT_ASSERT_EQUAL (TrackerUtils::parse_pitch ("C4", 9), TrackerUtils::parse_pitch ("C4", 2));
	/* invalid names yield 255 */
	CPPUNIT_ASSERT_EQUAL ((uint8_t) 255, TrackerUtils::parse_pitch ("Z", 4));
}

void
TrackerUtilsTest::testChannel ()
{
	CPPUNIT_ASSERT_EQUAL (std::string ("0"), TrackerUtils::channel_to_string (0, 16));
	CPPUNIT_ASSERT_EQUAL (std::string ("1"), TrackerUtils::channel_to_string (0, 10));
	CPPUNIT_ASSERT_EQUAL (std::string ("F"), TrackerUtils::channel_to_string (15, 16));
	CPPUNIT_ASSERT_EQUAL (std::string ("16"), TrackerUtils::channel_to_string (15, 10));

	CPPUNIT_ASSERT_EQUAL ((uint8_t) 0, TrackerUtils::string_to_channel ("0", 16));
	CPPUNIT_ASSERT_EQUAL ((uint8_t) 0, TrackerUtils::string_to_channel ("1", 10));
	CPPUNIT_ASSERT_EQUAL ((uint8_t) 15, TrackerUtils::string_to_channel ("F", 16));
}

void
TrackerUtilsTest::testFormatting ()
{
	CPPUNIT_ASSERT_EQUAL (std::string ("<u>x</u>"), TrackerUtils::underline ("x"));
	CPPUNIT_ASSERT_EQUAL (std::string ("<b>y</b>"), TrackerUtils::bold ("y"));

	CPPUNIT_ASSERT_EQUAL (std::string ("#112233"), TrackerUtils::color_to_string (0x11223300));
	CPPUNIT_ASSERT_EQUAL (std::string ("#000000"), TrackerUtils::color_to_string (0));

	Temporal::BBT_Time bbt;
	bbt.bars = 1;
	bbt.beats = 2;
	bbt.ticks = 3;
	CPPUNIT_ASSERT_EQUAL (std::string ("001|2|003"), TrackerUtils::bbt_to_string (bbt, 16));

	IDParameter id_param (PBD::ID (0), Evoral::Parameter (0));
	CPPUNIT_ASSERT (TrackerUtils::id_param_to_string (id_param).find ("(id=") == 0);
}

void
TrackerUtilsTest::testNoteAndEventEquality ()
{
	NotePtr a (new NoteType (0, Temporal::Beats::from_double (1.0), Temporal::Beats::from_double (0.5), 60, 100));
	NotePtr b (new NoteType (0, Temporal::Beats::from_double (1.0), Temporal::Beats::from_double (0.5), 60, 100));
	NotePtr c (new NoteType (1, Temporal::Beats::from_double (1.0), Temporal::Beats::from_double (0.5), 60, 100));
	NotePtr d (new NoteType (0, Temporal::Beats::from_double (1.5), Temporal::Beats::from_double (0.5), 60, 100));

	CPPUNIT_ASSERT (TrackerUtils::is_on_equal (a, b));
	CPPUNIT_ASSERT (!TrackerUtils::is_on_equal (a, c)); /* different channel */
	CPPUNIT_ASSERT (!TrackerUtils::is_on_equal (a, d)); /* different on time */
	CPPUNIT_ASSERT (TrackerUtils::is_off_equal (a, b));
	CPPUNIT_ASSERT (!TrackerUtils::is_off_equal (a, d)); /* different off time */
	/* a ends at 1.5 which is where d starts */
	CPPUNIT_ASSERT (TrackerUtils::off_meets_on (a, d));
	CPPUNIT_ASSERT (!TrackerUtils::off_meets_on (d, a));

	Evoral::ControlEvent e1 (Temporal::timepos_t (Temporal::Beats::from_double (1.0)), 0.5);
	Evoral::ControlEvent e2 (Temporal::timepos_t (Temporal::Beats::from_double (1.0)), 0.5);
	Evoral::ControlEvent e3 (Temporal::timepos_t (Temporal::Beats::from_double (1.0)), 0.25);
	Evoral::ControlEvent e4 (Temporal::timepos_t (Temporal::Beats::from_double (2.0)), 0.5);
	CPPUNIT_ASSERT (TrackerUtils::is_equal (e1, e2));
	CPPUNIT_ASSERT (!TrackerUtils::is_equal (e1, e3));
	CPPUNIT_ASSERT (!TrackerUtils::is_equal (e1, e4));
}

void
TrackerUtilsTest::testContainerAndClampHelpers ()
{
	std::set<int> s;
	s.insert (1);
	s.insert (2);
	CPPUNIT_ASSERT (TrackerUtils::is_in (1, s));
	CPPUNIT_ASSERT (!TrackerUtils::is_in (3, s));

	std::map<int, int> m;
	m[1] = 10;
	CPPUNIT_ASSERT (TrackerUtils::is_key_in (1, m));
	CPPUNIT_ASSERT (!TrackerUtils::is_key_in (2, m));

	CPPUNIT_ASSERT_EQUAL (3, TrackerUtils::clamp (5, 1, 3));
	CPPUNIT_ASSERT_EQUAL (1, TrackerUtils::clamp (0, 1, 3));
	CPPUNIT_ASSERT_EQUAL (2, TrackerUtils::clamp (2, 1, 3));
}
