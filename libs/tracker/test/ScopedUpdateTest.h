#ifndef __ardour_tracker_test_scoped_update_test_h_
#define __ardour_tracker_test_scoped_update_test_h_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace ARDOUR { class Session; }

class ScopedUpdateTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (ScopedUpdateTest);

	CPPUNIT_TEST (testAudioAutomationScopedDiff);

	CPPUNIT_TEST_SUITE_END ();

public:
	void setUp ();
	void tearDown ();

	void testAudioAutomationScopedDiff ();

private:
	ARDOUR::Session* _session;
};

#endif /* __ardour_tracker_test_scoped_update_test_h_ */
